/**
 * =============================================================================
 * Chanux OS - VGA Text Mode Driver Header
 * =============================================================================
 * Provides basic text output to VGA text mode buffer.
 * Standard 80x25 text mode with 16 colors.
 * =============================================================================
 */

#ifndef CHANUX_VGA_H
#define CHANUX_VGA_H

#include "../../include/types.h"

/* =============================================================================
 * VGA Constants
 * =============================================================================
 */

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000

/* =============================================================================
 * VGA Colors
 * =============================================================================
 */

typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

/* =============================================================================
 * VGA Functions
 * =============================================================================
 */

/**
 * Initialize the VGA driver.
 * Sets default colors and clears the screen.
 */
void vga_init(void);

/**
 * Clear the entire screen with current background color.
 */
void vga_clear(void);

/**
 * Set the foreground and background colors.
 *
 * @param fg Foreground color (text color)
 * @param bg Background color
 */
void vga_set_color(vga_color_t fg, vga_color_t bg);

/**
 * Set the cursor position.
 *
 * @param x Column (0 to VGA_WIDTH-1)
 * @param y Row (0 to VGA_HEIGHT-1)
 */
void vga_set_cursor(int x, int y);

/**
 * Get the current cursor X position.
 *
 * @return Current column
 */
int vga_get_cursor_x(void);

/**
 * Get the current cursor Y position.
 *
 * @return Current row
 */
int vga_get_cursor_y(void);

/**
 * Print a single character at current cursor position.
 * Automatically advances cursor and handles newlines.
 *
 * @param c Character to print
 */
void vga_putchar(char c);

/**
 * Print a null-terminated string.
 *
 * @param str String to print
 */
void vga_puts(const char* str);

/**
 * Print a string with a newline at the end.
 *
 * @param str String to print
 */
void vga_println(const char* str);

/**
 * Scroll the screen up by one line.
 */
void vga_scroll(void);

/**
 * Update the hardware cursor position to match software cursor.
 */
void vga_update_cursor(void);

/**
 * Enable or disable the hardware cursor.
 *
 * @param enabled true to enable, false to disable
 */
void vga_enable_cursor(bool enabled);

/* =============================================================================
 * Printing Functions
 * =============================================================================
 */

/**
 * Print an unsigned integer in decimal.
 *
 * @param value Value to print
 */
void vga_print_dec(uint64_t value);

/**
 * Print an unsigned integer in hexadecimal.
 *
 * @param value Value to print
 */
void vga_print_hex(uint64_t value);

/**
 * Simple printf-like function.
 * Supports: %s (string), %c (char), %d (decimal), %x (hex), %p (pointer), %% (percent)
 *
 * @param format Format string
 * @param ... Arguments
 */
void kprintf(const char* format, ...);

#endif /* CHANUX_VGA_H */
