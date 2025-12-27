/**
 * =============================================================================
 * Chanux OS - PS/2 Keyboard Driver
 * =============================================================================
 * Handles PS/2 keyboard input via IRQ1.
 *
 * Features:
 *   - Scancode set 1 to ASCII conversion
 *   - Circular input buffer for key storage
 *   - Modifier key tracking (Shift, Ctrl, Alt)
 *   - Blocking and non-blocking read functions
 * =============================================================================
 */

#ifndef CHANUX_KEYBOARD_H
#define CHANUX_KEYBOARD_H

#include "../types.h"

/* =============================================================================
 * Keyboard I/O Ports
 * =============================================================================
 */

#define KB_DATA_PORT        0x60    /* Keyboard data port */
#define KB_STATUS_PORT      0x64    /* Keyboard status port (read) */
#define KB_COMMAND_PORT     0x64    /* Keyboard command port (write) */

/* =============================================================================
 * Keyboard Status Register Bits
 * =============================================================================
 */

#define KB_STATUS_OUTPUT    0x01    /* Output buffer full (data available) */
#define KB_STATUS_INPUT     0x02    /* Input buffer full (controller busy) */
#define KB_STATUS_SYSTEM    0x04    /* System flag */
#define KB_STATUS_CMD       0x08    /* Command/data (0=data, 1=command) */
#define KB_STATUS_TIMEOUT   0x40    /* Timeout error */
#define KB_STATUS_PARITY    0x80    /* Parity error */

/* =============================================================================
 * Special Scancodes
 * =============================================================================
 */

#define KB_RELEASE_BIT      0x80    /* Bit 7 set means key release */

/* Key scancodes (make codes) */
#define KB_SC_LSHIFT        0x2A    /* Left Shift */
#define KB_SC_RSHIFT        0x36    /* Right Shift */
#define KB_SC_CTRL          0x1D    /* Control */
#define KB_SC_ALT           0x38    /* Alt */
#define KB_SC_CAPS          0x3A    /* Caps Lock */
#define KB_SC_ESC           0x01    /* Escape */
#define KB_SC_ENTER         0x1C    /* Enter */
#define KB_SC_BACKSPACE     0x0E    /* Backspace */
#define KB_SC_TAB           0x0F    /* Tab */
#define KB_SC_SPACE         0x39    /* Space */

/* =============================================================================
 * Buffer Configuration
 * =============================================================================
 */

#define KB_BUFFER_SIZE      256     /* Circular buffer size */

/* =============================================================================
 * Keyboard Functions
 * =============================================================================
 */

/**
 * Initialize the PS/2 keyboard.
 * Sets up IRQ1 handler and clears the buffer.
 */
void keyboard_init(void);

/**
 * Check if a key is available in the buffer.
 *
 * @return true if at least one key is available
 */
bool keyboard_has_key(void);

/**
 * Get the next character from the keyboard buffer.
 * Blocks until a key is available.
 *
 * @return ASCII character code
 */
char keyboard_getchar(void);

/**
 * Get the next character from the keyboard buffer (non-blocking).
 *
 * @return ASCII character code, or 0 if no key available
 */
char keyboard_getchar_nonblock(void);

/**
 * Check if Shift key is currently pressed.
 *
 * @return true if Shift is pressed
 */
bool keyboard_is_shift_pressed(void);

/**
 * Check if Ctrl key is currently pressed.
 *
 * @return true if Ctrl is pressed
 */
bool keyboard_is_ctrl_pressed(void);

/**
 * Check if Alt key is currently pressed.
 *
 * @return true if Alt is pressed
 */
bool keyboard_is_alt_pressed(void);

/**
 * Check if Caps Lock is active.
 *
 * @return true if Caps Lock is on
 */
bool keyboard_is_caps_lock(void);

#endif /* CHANUX_KEYBOARD_H */
