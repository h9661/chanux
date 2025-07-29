/*
 * PS/2 Keyboard Driver
 * 
 * This file defines the interface for the PS/2 keyboard driver.
 * It handles scancode translation, special keys, and provides
 * a buffered input system for reading keypresses.
 */

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

/* Keyboard I/O Ports */
#define KEYBOARD_DATA_PORT      0x60    /* Read scancodes/Send commands */
#define KEYBOARD_STATUS_PORT    0x64    /* Status register */
#define KEYBOARD_COMMAND_PORT   0x64    /* Command register (write) */

/* Keyboard Commands */
#define KEYBOARD_CMD_SET_LEDS   0xED    /* Set keyboard LEDs */
#define KEYBOARD_CMD_ECHO       0xEE    /* Echo command for testing */
#define KEYBOARD_CMD_SET_RATE   0xF3    /* Set typematic rate/delay */
#define KEYBOARD_CMD_ENABLE     0xF4    /* Enable scanning */
#define KEYBOARD_CMD_DISABLE    0xF5    /* Disable scanning */
#define KEYBOARD_CMD_RESET      0xFF    /* Reset keyboard */

/* Keyboard Responses */
#define KEYBOARD_RESP_ACK       0xFA    /* Command acknowledged */
#define KEYBOARD_RESP_RESEND    0xFE    /* Resend last command */
#define KEYBOARD_RESP_ERROR     0xFC    /* Error */

/* Status Register Bits */
#define KEYBOARD_STATUS_OUTPUT_FULL  0x01   /* Output buffer full */
#define KEYBOARD_STATUS_INPUT_FULL   0x02   /* Input buffer full */

/* Special Scancodes */
#define SCANCODE_EXTENDED       0xE0    /* Extended scancode prefix */
#define SCANCODE_RELEASE        0xF0    /* Key release prefix (set 2) */
#define SCANCODE_PAUSE_START    0xE1    /* Pause key sequence start */

/* Special Keys */
#define KEY_ESC         0x1B
#define KEY_BACKSPACE   0x08
#define KEY_TAB         0x09
#define KEY_ENTER       0x0A
#define KEY_CTRL        0x00    /* Non-printable */
#define KEY_SHIFT       0x00    /* Non-printable */
#define KEY_ALT         0x00    /* Non-printable */
#define KEY_CAPSLOCK    0x00    /* Non-printable */
#define KEY_F1          0x00    /* Function keys TBD */
#define KEY_F2          0x00
#define KEY_F3          0x00
#define KEY_F4          0x00
#define KEY_F5          0x00
#define KEY_F6          0x00
#define KEY_F7          0x00
#define KEY_F8          0x00
#define KEY_F9          0x00
#define KEY_F10         0x00
#define KEY_F11         0x00
#define KEY_F12         0x00

/* Modifier Key Flags */
#define MOD_SHIFT_LEFT  0x01
#define MOD_SHIFT_RIGHT 0x02
#define MOD_CTRL_LEFT   0x04
#define MOD_CTRL_RIGHT  0x08
#define MOD_ALT_LEFT    0x10
#define MOD_ALT_RIGHT   0x20
#define MOD_CAPSLOCK    0x40
#define MOD_NUMLOCK     0x80

/* Keyboard Buffer Size */
#define KEYBOARD_BUFFER_SIZE    256

/* Key Event Structure */
typedef struct {
    uint8_t scancode;       /* Raw scancode */
    uint8_t ascii;          /* ASCII character (0 if non-printable) */
    uint8_t modifiers;      /* Active modifier keys */
    bool pressed;           /* true = pressed, false = released */
} key_event_t;

/* Function Declarations */

/* Initialize the keyboard driver */
void keyboard_init(void);

/* Enable/disable keyboard */
void keyboard_enable(void);
void keyboard_disable(void);

/* Keyboard interrupt handler (called from IRQ1) */
void keyboard_interrupt_handler(void);

/* Check if keyboard has data available */
bool keyboard_has_key(void);

/* Read a key event from the buffer */
key_event_t keyboard_read_key(void);

/* Read a character (blocks until available) */
char keyboard_getchar(void);

/* Get current modifier state */
uint8_t keyboard_get_modifiers(void);

/* Set keyboard LEDs */
void keyboard_set_leds(bool caps, bool num, bool scroll);

/* Flush keyboard buffer */
void keyboard_flush(void);

/* Get buffer fill level */
uint32_t keyboard_buffer_count(void);

/* Scancode to ASCII lookup tables (US layout) */
extern const uint8_t scancode_to_ascii[];
extern const uint8_t scancode_to_ascii_shift[];

#endif /* _KEYBOARD_H */