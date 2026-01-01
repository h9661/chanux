/**
 * =============================================================================
 * Chanux OS - VGA Text Mode Driver Implementation
 * =============================================================================
 * Provides basic text output to VGA text mode buffer (0xB8000).
 *
 * VGA Text Mode Memory Layout:
 *   Each character cell is 2 bytes:
 *   - Byte 0: ASCII character code
 *   - Byte 1: Attribute byte (foreground | background << 4)
 *
 * Screen is 80 columns x 25 rows = 4000 bytes
 * =============================================================================
 */

#include "vga.h"
#include "../../include/kernel.h"
#include "../../include/stdarg.h"

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/* VGA buffer pointer */
static volatile uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;

/* Current cursor position */
static int cursor_x = 0;
static int cursor_y = 0;

/* Current color attribute */
static uint8_t current_color = 0;

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * Create a VGA color attribute byte.
 */
static inline uint8_t vga_make_color(vga_color_t fg, vga_color_t bg) {
    return fg | (bg << 4);
}

/**
 * Create a VGA character entry (character + color).
 */
static inline uint16_t vga_make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/**
 * Calculate buffer index from x, y coordinates.
 */
static inline size_t vga_index(int x, int y) {
    return y * VGA_WIDTH + x;
}

/* =============================================================================
 * VGA I/O Ports for Cursor Control
 * =============================================================================
 */

#define VGA_CTRL_REGISTER   0x3D4
#define VGA_DATA_REGISTER   0x3D5

/* =============================================================================
 * Serial Port for Debug Output
 * =============================================================================
 */

#define SERIAL_PORT         0x3F8

static int serial_initialized = 0;

static void serial_init(void) {
    outb(SERIAL_PORT + 1, 0x00);    /* Disable all interrupts */
    outb(SERIAL_PORT + 3, 0x80);    /* Enable DLAB (set baud rate divisor) */
    outb(SERIAL_PORT + 0, 0x03);    /* Set divisor to 3 (lo byte) 38400 baud */
    outb(SERIAL_PORT + 1, 0x00);    /*                  (hi byte) */
    outb(SERIAL_PORT + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(SERIAL_PORT + 2, 0xC7);    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(SERIAL_PORT + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */
    serial_initialized = 1;
}

static void serial_putchar(char c) {
    if (!serial_initialized) {
        serial_init();
    }
    /* Wait for transmit buffer to be empty */
    while ((inb(SERIAL_PORT + 5) & 0x20) == 0);
    outb(SERIAL_PORT, c);
}

/* =============================================================================
 * Implementation
 * =============================================================================
 */

void vga_init(void) {
    /* Set default colors: light grey on black */
    current_color = vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    /* Clear screen */
    vga_clear();

    /* Enable cursor */
    vga_enable_cursor(true);
}

void vga_clear(void) {
    uint16_t blank = vga_make_entry(' ', current_color);

    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }

    cursor_x = 0;
    cursor_y = 0;
    vga_update_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    current_color = vga_make_color(fg, bg);
}

void vga_set_cursor(int x, int y) {
    if (x >= 0 && x < VGA_WIDTH) {
        cursor_x = x;
    }
    if (y >= 0 && y < VGA_HEIGHT) {
        cursor_y = y;
    }
    vga_update_cursor();
}

int vga_get_cursor_x(void) {
    return cursor_x;
}

int vga_get_cursor_y(void) {
    return cursor_y;
}

void vga_scroll(void) {
    /* Move all lines up by one */
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[vga_index(x, y)] = vga_buffer[vga_index(x, y + 1)];
        }
    }

    /* Clear the last line */
    uint16_t blank = vga_make_entry(' ', current_color);
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[vga_index(x, VGA_HEIGHT - 1)] = blank;
    }

    /* Move cursor up */
    cursor_y = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    /* Mirror output to serial for debugging */
    serial_putchar(c);

    /* Handle special characters */
    switch (c) {
        case '\n':
            /* Newline: move to start of next line */
            cursor_x = 0;
            cursor_y++;
            break;

        case '\r':
            /* Carriage return: move to start of current line */
            cursor_x = 0;
            break;

        case '\t':
            /* Tab: move to next tab stop (every 8 columns) */
            cursor_x = (cursor_x + 8) & ~7;
            break;

        case '\b':
            /* Backspace: move cursor back and clear character */
            if (cursor_x > 0) {
                cursor_x--;
                vga_buffer[vga_index(cursor_x, cursor_y)] =
                    vga_make_entry(' ', current_color);
            }
            break;

        default:
            /* Regular character: print it */
            if (c >= ' ') {  /* Only printable characters */
                vga_buffer[vga_index(cursor_x, cursor_y)] =
                    vga_make_entry(c, current_color);
                cursor_x++;
            }
            break;
    }

    /* Handle line wrap */
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    /* Handle scroll */
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }

    vga_update_cursor();
}

void vga_puts(const char* str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

void vga_println(const char* str) {
    vga_puts(str);
    vga_putchar('\n');
}

void vga_update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;

    /* Send cursor position to VGA controller */
    outb(VGA_CTRL_REGISTER, 0x0F);          /* Low byte index */
    outb(VGA_DATA_REGISTER, pos & 0xFF);    /* Low byte value */
    outb(VGA_CTRL_REGISTER, 0x0E);          /* High byte index */
    outb(VGA_DATA_REGISTER, (pos >> 8) & 0xFF);  /* High byte value */
}

void vga_enable_cursor(bool enabled) {
    if (enabled) {
        /* Enable cursor, set start and end scanlines */
        outb(VGA_CTRL_REGISTER, 0x0A);
        outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xC0) | 14);

        outb(VGA_CTRL_REGISTER, 0x0B);
        outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xE0) | 15);
    } else {
        /* Disable cursor */
        outb(VGA_CTRL_REGISTER, 0x0A);
        outb(VGA_DATA_REGISTER, 0x20);
    }
}

/* =============================================================================
 * Printing Functions
 * =============================================================================
 */

void vga_print_dec(uint64_t value) {
    char buffer[21];  /* Max 20 digits for 64-bit + null */
    int i = 0;

    if (value == 0) {
        vga_putchar('0');
        return;
    }

    /* Build string in reverse */
    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    /* Print in correct order */
    while (i > 0) {
        vga_putchar(buffer[--i]);
    }
}

void vga_print_hex(uint64_t value) {
    static const char hex_chars[] = "0123456789ABCDEF";
    char buffer[17];  /* Max 16 hex digits + null */
    int i = 0;

    if (value == 0) {
        vga_puts("0x0");
        return;
    }

    /* Build string in reverse */
    while (value > 0) {
        buffer[i++] = hex_chars[value & 0xF];
        value >>= 4;
    }

    /* Print prefix and value */
    vga_puts("0x");
    while (i > 0) {
        vga_putchar(buffer[--i]);
    }
}

/* =============================================================================
 * kprintf Implementation
 * =============================================================================
 */

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format == '%') {
            format++;

            switch (*format) {
                case 's': {
                    /* String */
                    const char* str = va_arg(args, const char*);
                    if (str) {
                        vga_puts(str);
                    } else {
                        vga_puts("(null)");
                    }
                    break;
                }

                case 'c': {
                    /* Character */
                    char c = (char)va_arg(args, int);
                    vga_putchar(c);
                    break;
                }

                case 'd':
                case 'i': {
                    /* Signed decimal */
                    int64_t value = va_arg(args, int64_t);
                    if (value < 0) {
                        vga_putchar('-');
                        value = -value;
                    }
                    vga_print_dec((uint64_t)value);
                    break;
                }

                case 'u': {
                    /* Unsigned decimal */
                    uint64_t value = va_arg(args, uint64_t);
                    vga_print_dec(value);
                    break;
                }

                case 'x':
                case 'X': {
                    /* Hexadecimal */
                    uint64_t value = va_arg(args, uint64_t);
                    vga_print_hex(value);
                    break;
                }

                case 'p': {
                    /* Pointer */
                    void* ptr = va_arg(args, void*);
                    vga_print_hex((uint64_t)ptr);
                    break;
                }

                case '%':
                    /* Percent sign */
                    vga_putchar('%');
                    break;

                default:
                    /* Unknown format, print as-is */
                    vga_putchar('%');
                    vga_putchar(*format);
                    break;
            }
        } else {
            vga_putchar(*format);
        }

        format++;
    }

    va_end(args);
}
