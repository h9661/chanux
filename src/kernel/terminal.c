/*
 * VGA Text Mode Terminal Driver
 *
 * This provides basic text output functionality using VGA text mode.
 * VGA text mode uses a memory-mapped buffer at 0xB8000 where each
 * character on screen is represented by 2 bytes: character + attributes.
 */

#include <stdint.h>
#include <stddef.h>

/* VGA text mode constants */
static const size_t VGA_WIDTH = 80;   /* Standard VGA text mode width */
static const size_t VGA_HEIGHT = 25;  /* Standard VGA text mode height */

/* Terminal state variables */
size_t terminal_row;        /* Current cursor row position */
size_t terminal_column;     /* Current cursor column position */
uint8_t terminal_color;     /* Current text color attributes */
uint16_t* terminal_buffer;  /* Pointer to VGA text buffer */

/* VGA color constants
 * These can be used for foreground and background colors
 */
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

/*
 * vga_entry_color - Create a color attribute byte
 * @fg: Foreground color (0-15)
 * @bg: Background color (0-15)
 *
 * Returns: 8-bit color attribute (foreground in low nibble, background in high)
 */
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

/*
 * vga_entry - Create a VGA text mode character entry
 * @uc: ASCII character to display
 * @color: Color attributes
 *
 * Returns: 16-bit value with character in low byte, attributes in high byte
 */
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

/*
 * strlen - Calculate string length
 * @str: Null-terminated string
 *
 * Returns: Length of string (not including null terminator)
 */
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

/*
 * terminal_initialize - Initialize the terminal
 *
 * Sets up the terminal with default colors and clears the screen.
 * Must be called before any other terminal functions.
 */
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    /* VGA text buffer is located at physical address 0xB8000 */
    terminal_buffer = (uint16_t*) 0xB8000;
    
    /* Clear the screen by filling it with spaces */
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

/*
 * terminal_setcolor - Change current text color
 * @color: New color attributes to use
 */
void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

/*
 * terminal_putentryat - Put a character at specific screen position
 * @c: Character to display
 * @color: Color attributes
 * @x: Column position (0-79)
 * @y: Row position (0-24)
 */
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

/*
 * terminal_scroll - Scroll the terminal up by one line
 *
 * Moves all lines up by one, discarding the top line and
 * creating a blank line at the bottom.
 */
void terminal_scroll(void) {
    /* Move each line up by one */
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t src_index = (y + 1) * VGA_WIDTH + x;
            const size_t dst_index = y * VGA_WIDTH + x;
            terminal_buffer[dst_index] = terminal_buffer[src_index];
        }
    }
    
    /* Clear the bottom line */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        terminal_buffer[index] = vga_entry(' ', terminal_color);
    }
    
    /* Keep cursor on the last line */
    terminal_row = VGA_HEIGHT - 1;
}

/*
 * terminal_putchar - Write a character to the terminal
 * @c: Character to write
 *
 * Handles newlines and scrolling when reaching bottom of screen.
 */
void terminal_putchar(char c) {
    if (c == '\n') {
        /* Newline: move to start of next line */
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
    } else {
        /* Regular character: put at current position */
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            /* Wrap to next line if we hit the right edge */
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT) {
                terminal_scroll();
            }
        }
    }
}

/*
 * terminal_write - Write a buffer of characters
 * @data: Buffer containing characters to write
 * @size: Number of characters to write
 */
void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

/*
 * terminal_writestring - Write a null-terminated string
 * @data: String to write
 */
void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}

/*
 * terminal_write_hex - Write a hexadecimal number
 * @value: Value to write in hexadecimal
 */
void terminal_write_hex(uint8_t value) {
    const char hex_chars[] = "0123456789ABCDEF";
    terminal_putchar(hex_chars[(value >> 4) & 0xF]);
    terminal_putchar(hex_chars[value & 0xF]);
}

/*
 * terminal_write_dec - Write a decimal number
 * @value: Value to write in decimal
 */
void terminal_write_dec(uint32_t value) {
    if (value == 0) {
        terminal_putchar('0');
        return;
    }
    
    /* Buffer to store digits (max 10 digits for 32-bit) */
    char buffer[11];
    int i = 0;
    
    /* Extract digits in reverse order */
    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    /* Print digits in correct order */
    while (i > 0) {
        terminal_putchar(buffer[--i]);
    }
}