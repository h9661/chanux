/**
 * =============================================================================
 * Chanux OS - PS/2 Keyboard Driver Implementation
 * =============================================================================
 * Handles keyboard input using scancode set 1 (default for PS/2 keyboards).
 *
 * Scancode set 1:
 *   - Key press: scancode (bit 7 = 0)
 *   - Key release: scancode | 0x80 (bit 7 = 1)
 *   - Special keys may have E0 or E1 prefix (not handled yet)
 * =============================================================================
 */

#include "../../include/drivers/keyboard.h"
#include "../../include/drivers/pic.h"
#include "../../include/interrupts/irq.h"
#include "../../include/kernel.h"

/* =============================================================================
 * Scancode to ASCII Tables
 * =============================================================================
 */

/* Scancode set 1 to ASCII (lowercase, unshifted) */
static const char scancode_ascii[128] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',   /* 0x00-0x07 */
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',  /* 0x08-0x0F */
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',   /* 0x10-0x17 */
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',   /* 0x18-0x1F */
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',   /* 0x20-0x27 */
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',   /* 0x28-0x2F */
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',   /* 0x30-0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,     /* 0x38-0x3F */
    0,    0,    0,    0,    0,    0,    0,    '7',   /* 0x40-0x47 */
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',   /* 0x48-0x4F */
    '2',  '3',  '0',  '.',  0,    0,    0,    0,     /* 0x50-0x57 */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x58-0x5F */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x60-0x67 */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x68-0x6F */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x70-0x77 */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x78-0x7F */
};

/* Scancode set 1 to ASCII (shifted) */
static const char scancode_ascii_shift[128] = {
    0,    27,   '!',  '@',  '#',  '$',  '%',  '^',   /* 0x00-0x07 */
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',  /* 0x08-0x0F */
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',   /* 0x10-0x17 */
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',   /* 0x18-0x1F */
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',   /* 0x20-0x27 */
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',   /* 0x28-0x2F */
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',   /* 0x30-0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,     /* 0x38-0x3F */
    0,    0,    0,    0,    0,    0,    0,    '7',   /* 0x40-0x47 */
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',   /* 0x48-0x4F */
    '2',  '3',  '0',  '.',  0,    0,    0,    0,     /* 0x50-0x57 */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x58-0x5F */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x60-0x67 */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x68-0x6F */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x70-0x77 */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x78-0x7F */
};

/* =============================================================================
 * Static Data
 * =============================================================================
 */

/* Circular input buffer */
static volatile char key_buffer[KB_BUFFER_SIZE];
static volatile size_t buffer_head = 0;  /* Write position */
static volatile size_t buffer_tail = 0;  /* Read position */

/* Modifier key states */
static volatile bool shift_pressed = false;
static volatile bool ctrl_pressed = false;
static volatile bool alt_pressed = false;
static volatile bool caps_lock = false;

/* =============================================================================
 * Buffer Operations
 * =============================================================================
 */

/**
 * Check if the buffer is empty.
 */
static inline bool buffer_empty(void) {
    return buffer_head == buffer_tail;
}

/**
 * Check if the buffer is full.
 */
static inline bool buffer_full(void) {
    return ((buffer_head + 1) % KB_BUFFER_SIZE) == buffer_tail;
}

/**
 * Add a character to the buffer.
 */
static void buffer_put(char c) {
    if (!buffer_full()) {
        key_buffer[buffer_head] = c;
        buffer_head = (buffer_head + 1) % KB_BUFFER_SIZE;
    }
}

/**
 * Get a character from the buffer.
 */
static char buffer_get(void) {
    if (buffer_empty()) {
        return 0;
    }
    char c = key_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

/* =============================================================================
 * Keyboard IRQ Handler
 * =============================================================================
 */

/**
 * Keyboard IRQ1 handler.
 */
static void keyboard_irq_handler(registers_t* regs) {
    (void)regs;

    /* Read scancode from keyboard data port */
    uint8_t scancode = inb(KB_DATA_PORT);

    /* Check if this is a key release */
    bool released = (scancode & KB_RELEASE_BIT) != 0;
    uint8_t key = scancode & 0x7F;  /* Remove release bit */

    /* Handle modifier keys */
    switch (key) {
        case KB_SC_LSHIFT:
        case KB_SC_RSHIFT:
            shift_pressed = !released;
            return;

        case KB_SC_CTRL:
            ctrl_pressed = !released;
            return;

        case KB_SC_ALT:
            alt_pressed = !released;
            return;

        case KB_SC_CAPS:
            if (!released) {
                caps_lock = !caps_lock;
            }
            return;
    }

    /* Only process key press events (not releases) */
    if (released) {
        return;
    }

    /* Look up ASCII value */
    char ascii;
    bool use_shift = shift_pressed;

    /* Caps lock affects only letters */
    if (caps_lock && key >= 0x10 && key <= 0x19) {  /* Q-P */
        use_shift = !use_shift;
    } else if (caps_lock && key >= 0x1E && key <= 0x26) {  /* A-L */
        use_shift = !use_shift;
    } else if (caps_lock && key >= 0x2C && key <= 0x32) {  /* Z-M */
        use_shift = !use_shift;
    }

    if (use_shift) {
        ascii = scancode_ascii_shift[key];
    } else {
        ascii = scancode_ascii[key];
    }

    /* Add to buffer if it's a printable character or control character */
    if (ascii != 0) {
        buffer_put(ascii);
    }
}

/* =============================================================================
 * Keyboard Initialization
 * =============================================================================
 */

/**
 * Initialize the PS/2 keyboard.
 */
void keyboard_init(void) {
    /* Clear the buffer */
    buffer_head = 0;
    buffer_tail = 0;

    /* Clear modifier states */
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
    caps_lock = false;

    /* Drain any pending data */
    while (inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT) {
        inb(KB_DATA_PORT);
    }

    /* Register IRQ1 handler */
    irq_register_handler(1, keyboard_irq_handler);

    /* Enable IRQ1 */
    pic_unmask_irq(1);
}

/* =============================================================================
 * Keyboard Input Functions
 * =============================================================================
 */

/**
 * Check if a key is available.
 */
bool keyboard_has_key(void) {
    return !buffer_empty();
}

/**
 * Get the next character (blocking).
 */
char keyboard_getchar(void) {
    /* Wait for a key */
    while (buffer_empty()) {
        halt();  /* Wait for interrupt */
    }
    return buffer_get();
}

/**
 * Get the next character (non-blocking).
 */
char keyboard_getchar_nonblock(void) {
    return buffer_get();
}

/* =============================================================================
 * Modifier Key State Functions
 * =============================================================================
 */

bool keyboard_is_shift_pressed(void) {
    return shift_pressed;
}

bool keyboard_is_ctrl_pressed(void) {
    return ctrl_pressed;
}

bool keyboard_is_alt_pressed(void) {
    return alt_pressed;
}

bool keyboard_is_caps_lock(void) {
    return caps_lock;
}
