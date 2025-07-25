/*
 * PS/2 Keyboard Driver Implementation
 * 
 * This driver handles PS/2 keyboard input, translates scancodes to ASCII,
 * manages modifier keys, and provides a buffered input system.
 */

#include <stdint.h>
#include <stdbool.h>
#include "../include/keyboard.h"
#include "../include/pic.h"

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_putchar(char c);
extern void terminal_write_hex(uint8_t value);

/* Keyboard state */
static uint8_t modifiers = 0;
static bool extended_scancode = false;
static bool caps_lock_on = false;
static bool num_lock_on = false;
static bool scroll_lock_on = false;

/* Circular buffer for keyboard input */
static key_event_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static uint32_t buffer_read_index = 0;
static uint32_t buffer_write_index = 0;
static uint32_t buffer_count = 0;

/* US QWERTY Scancode to ASCII lookup table (Set 1) */
/* Index is scancode, value is ASCII character */
const uint8_t scancode_to_ascii[128] = {
    0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',  // 0x00-0x07
    '7',  '8',  '9',  '0',  '-',  '=',  0x08, 0x09, // 0x08-0x0F
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  // 0x10-0x17
    'o',  'p',  '[',  ']',  0x0A, 0,    'a',  's',  // 0x18-0x1F
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  // 0x20-0x27
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',  // 0x28-0x2F
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  // 0x30-0x37
    0,    ' ',  0,    0,    0,    0,    0,    0,    // 0x38-0x3F
    0,    0,    0,    0,    0,    0,    0,    '7',  // 0x40-0x47
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',  // 0x48-0x4F
    '2',  '3',  '0',  '.',  0,    0,    0,    0,    // 0x50-0x57
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x58-0x5F
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x60-0x67
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x68-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x70-0x77
    0,    0,    0,    0,    0,    0,    0,    0     // 0x78-0x7F
};

/* US QWERTY Scancode to ASCII with Shift pressed */
const uint8_t scancode_to_ascii_shift[128] = {
    0,    0x1B, '!',  '@',  '#',  '$',  '%',  '^',  // 0x00-0x07
    '&',  '*',  '(',  ')',  '_',  '+',  0x08, 0x09, // 0x08-0x0F
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  // 0x10-0x17
    'O',  'P',  '{',  '}',  0x0A, 0,    'A',  'S',  // 0x18-0x1F
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  // 0x20-0x27
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',  // 0x28-0x2F
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  // 0x30-0x37
    0,    ' ',  0,    0,    0,    0,    0,    0,    // 0x38-0x3F
    0,    0,    0,    0,    0,    0,    0,    '7',  // 0x40-0x47
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',  // 0x48-0x4F
    '2',  '3',  '0',  '.',  0,    0,    0,    0,    // 0x50-0x57
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x58-0x5F
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x60-0x67
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x68-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x70-0x77
    0,    0,    0,    0,    0,    0,    0,    0     // 0x78-0x7F
};

/*
 * Wait for keyboard controller to be ready
 */
static void keyboard_wait_input(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (!(inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_INPUT_FULL)) {
            return;
        }
    }
}

/*
 * Wait for keyboard data to be available
 */
static void keyboard_wait_output(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_FULL) {
            return;
        }
    }
}

/*
 * Send command to keyboard
 */
static void keyboard_send_command(uint8_t command) {
    keyboard_wait_input();
    outb(KEYBOARD_DATA_PORT, command);
}

/*
 * Add key event to buffer
 */
static void keyboard_buffer_push(key_event_t event) {
    if (buffer_count < KEYBOARD_BUFFER_SIZE) {
        keyboard_buffer[buffer_write_index] = event;
        buffer_write_index = (buffer_write_index + 1) % KEYBOARD_BUFFER_SIZE;
        buffer_count++;
    }
    /* If buffer is full, drop the event */
}

/*
 * Initialize keyboard driver
 */
void keyboard_init(void) {
    terminal_writestring("Initializing keyboard driver...\n");
    
    /* Disable keyboard during initialization */
    keyboard_send_command(KEYBOARD_CMD_DISABLE);
    
    /* Flush any pending data */
    while (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_FULL) {
        inb(KEYBOARD_DATA_PORT);
    }
    
    /* Enable keyboard */
    keyboard_send_command(KEYBOARD_CMD_ENABLE);
    
    /* Wait for ACK */
    keyboard_wait_output();
    if (inb(KEYBOARD_DATA_PORT) != KEYBOARD_RESP_ACK) {
        terminal_writestring("Keyboard: Failed to enable\n");
        return;
    }
    
    /* Set LEDs off initially */
    keyboard_set_leds(false, false, false);
    
    /* Enable keyboard interrupt (IRQ1) */
    pic_enable_irq(1);
    
    terminal_writestring("Keyboard driver initialized\n");
}

/*
 * Enable keyboard
 */
void keyboard_enable(void) {
    keyboard_send_command(KEYBOARD_CMD_ENABLE);
    pic_enable_irq(1);
}

/*
 * Disable keyboard
 */
void keyboard_disable(void) {
    keyboard_send_command(KEYBOARD_CMD_DISABLE);
    pic_disable_irq(1);
}

/*
 * Process scancode and update modifiers
 */
static key_event_t process_scancode(uint8_t scancode) {
    key_event_t event = {0};
    event.scancode = scancode;
    event.modifiers = modifiers;
    
    /* Check if this is a key release (bit 7 set) */
    bool released = (scancode & 0x80) != 0;
    scancode &= 0x7F;  /* Clear release bit */
    
    event.pressed = !released;
    
    /* Handle modifier keys */
    switch (scancode) {
        case 0x2A:  /* Left Shift */
            if (released) modifiers &= ~MOD_SHIFT_LEFT;
            else modifiers |= MOD_SHIFT_LEFT;
            return event;
            
        case 0x36:  /* Right Shift */
            if (released) modifiers &= ~MOD_SHIFT_RIGHT;
            else modifiers |= MOD_SHIFT_RIGHT;
            return event;
            
        case 0x1D:  /* Left Ctrl */
            if (released) modifiers &= ~MOD_CTRL_LEFT;
            else modifiers |= MOD_CTRL_LEFT;
            return event;
            
        case 0x38:  /* Left Alt */
            if (released) modifiers &= ~MOD_ALT_LEFT;
            else modifiers |= MOD_ALT_LEFT;
            return event;
            
        case 0x3A:  /* Caps Lock */
            if (!released) {
                caps_lock_on = !caps_lock_on;
                if (caps_lock_on) modifiers |= MOD_CAPSLOCK;
                else modifiers &= ~MOD_CAPSLOCK;
                keyboard_set_leds(caps_lock_on, num_lock_on, scroll_lock_on);
            }
            return event;
            
        case 0x45:  /* Num Lock */
            if (!released) {
                num_lock_on = !num_lock_on;
                if (num_lock_on) modifiers |= MOD_NUMLOCK;
                else modifiers &= ~MOD_NUMLOCK;
                keyboard_set_leds(caps_lock_on, num_lock_on, scroll_lock_on);
            }
            return event;
    }
    
    /* Only process ASCII for key press events */
    if (!released && scancode < 128) {
        bool shift_pressed = (modifiers & (MOD_SHIFT_LEFT | MOD_SHIFT_RIGHT)) != 0;
        
        if (shift_pressed) {
            event.ascii = scancode_to_ascii_shift[scancode];
        } else {
            event.ascii = scancode_to_ascii[scancode];
        }
        
        /* Apply Caps Lock to letters */
        if (caps_lock_on && event.ascii >= 'a' && event.ascii <= 'z') {
            event.ascii = event.ascii - 'a' + 'A';
        } else if (caps_lock_on && event.ascii >= 'A' && event.ascii <= 'Z') {
            event.ascii = event.ascii - 'A' + 'a';
        }
    }
    
    return event;
}

/*
 * Keyboard interrupt handler
 */
void keyboard_interrupt_handler(void) {
    /* Read scancode from data port */
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    /* Handle extended scancodes */
    if (scancode == SCANCODE_EXTENDED) {
        extended_scancode = true;
        return;
    }
    
    /* Process the scancode */
    key_event_t event = process_scancode(scancode);
    
    /* Add to buffer if it's a valid event */
    if (event.pressed || event.scancode != 0) {
        keyboard_buffer_push(event);
    }
    
    /* Reset extended flag */
    extended_scancode = false;
}

/*
 * Check if keyboard has data
 */
bool keyboard_has_key(void) {
    return buffer_count > 0;
}

/*
 * Read key event from buffer
 */
key_event_t keyboard_read_key(void) {
    key_event_t event = {0};
    
    if (buffer_count > 0) {
        event = keyboard_buffer[buffer_read_index];
        buffer_read_index = (buffer_read_index + 1) % KEYBOARD_BUFFER_SIZE;
        buffer_count--;
    }
    
    return event;
}

/*
 * Read a character (blocking)
 */
char keyboard_getchar(void) {
    while (1) {
        if (keyboard_has_key()) {
            key_event_t event = keyboard_read_key();
            if (event.pressed && event.ascii != 0) {
                return event.ascii;
            }
        }
        /* Wait for interrupt */
        __asm__ __volatile__ ("hlt");
    }
}

/*
 * Get current modifiers
 */
uint8_t keyboard_get_modifiers(void) {
    return modifiers;
}

/*
 * Set keyboard LEDs
 */
void keyboard_set_leds(bool caps, bool num, bool scroll) {
    uint8_t led_byte = 0;
    if (scroll) led_byte |= 0x01;
    if (num) led_byte |= 0x02;
    if (caps) led_byte |= 0x04;
    
    keyboard_send_command(KEYBOARD_CMD_SET_LEDS);
    keyboard_wait_output();
    inb(KEYBOARD_DATA_PORT);  /* Read ACK */
    
    keyboard_send_command(led_byte);
    keyboard_wait_output();
    inb(KEYBOARD_DATA_PORT);  /* Read ACK */
}

/*
 * Flush keyboard buffer
 */
void keyboard_flush(void) {
    buffer_read_index = 0;
    buffer_write_index = 0;
    buffer_count = 0;
}

/*
 * Get buffer fill level
 */
uint32_t keyboard_buffer_count(void) {
    return buffer_count;
}