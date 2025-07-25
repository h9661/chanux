/*
 * Keyboard Driver Test
 * 
 * This file contains test code for the keyboard driver,
 * demonstrating various features like character input,
 * modifier keys, and special key handling.
 */

#include <stdint.h>
#include <stdbool.h>
#include "../include/keyboard.h"

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_putchar(char c);
extern void terminal_write_hex(uint8_t value);

/*
 * Print modifier state
 */
static void print_modifiers(uint8_t mods) {
    terminal_writestring("Modifiers: ");
    if (mods & MOD_SHIFT_LEFT) terminal_writestring("LSHIFT ");
    if (mods & MOD_SHIFT_RIGHT) terminal_writestring("RSHIFT ");
    if (mods & MOD_CTRL_LEFT) terminal_writestring("LCTRL ");
    if (mods & MOD_ALT_LEFT) terminal_writestring("LALT ");
    if (mods & MOD_CAPSLOCK) terminal_writestring("CAPS ");
    if (mods & MOD_NUMLOCK) terminal_writestring("NUM ");
    if (mods == 0) terminal_writestring("None");
    terminal_writestring("\n");
}

/*
 * Test basic character input
 */
static void test_character_input(void) {
    terminal_writestring("\n=== Character Input Test ===\n");
    terminal_writestring("Type some text (press ESC to continue):\n> ");
    
    keyboard_flush();
    
    while (1) {
        char ch = keyboard_getchar();
        
        if (ch == 0x1B) {  /* ESC */
            terminal_writestring("\n");
            break;
        } else if (ch == '\n') {
            terminal_writestring("\n> ");
        } else if (ch == 0x08) {  /* Backspace */
            /* Simple backspace handling */
            terminal_writestring("\b \b");
        } else {
            terminal_putchar(ch);
        }
    }
}

/*
 * Test modifier keys
 */
static void test_modifiers(void) {
    terminal_writestring("\n=== Modifier Keys Test ===\n");
    terminal_writestring("Press modifier keys (Shift, Ctrl, Alt, Caps Lock)\n");
    terminal_writestring("Press 'q' to continue\n\n");
    
    keyboard_flush();
    uint8_t last_mods = 0;
    
    while (1) {
        if (keyboard_has_key()) {
            key_event_t event = keyboard_read_key();
            
            /* Show modifier changes */
            if (event.modifiers != last_mods) {
                print_modifiers(event.modifiers);
                last_mods = event.modifiers;
            }
            
            /* Exit on 'q' */
            if (event.pressed && (event.ascii == 'q' || event.ascii == 'Q')) {
                break;
            }
        }
        __asm__ __volatile__ ("hlt");
    }
}

/*
 * Test raw scancode display
 */
static void test_scancodes(void) {
    terminal_writestring("\n=== Scancode Test ===\n");
    terminal_writestring("Press keys to see their scancodes\n");
    terminal_writestring("Press F1 to continue\n\n");
    
    keyboard_flush();
    
    while (1) {
        if (keyboard_has_key()) {
            key_event_t event = keyboard_read_key();
            
            terminal_writestring("Scancode: 0x");
            terminal_write_hex(event.scancode);
            terminal_writestring(event.pressed ? " (pressed)" : " (released)");
            
            if (event.ascii != 0) {
                terminal_writestring(" ASCII: '");
                terminal_putchar(event.ascii);
                terminal_writestring("' (0x");
                terminal_write_hex(event.ascii);
                terminal_writestring(")");
            }
            
            terminal_writestring("\n");
            
            /* Exit on F1 (scancode 0x3B) */
            if (event.pressed && event.scancode == 0x3B) {
                break;
            }
        }
        __asm__ __volatile__ ("hlt");
    }
}

/*
 * Test keyboard buffer
 */
static void test_buffer(void) {
    terminal_writestring("\n=== Buffer Test ===\n");
    terminal_writestring("Type quickly, then press Enter to see buffered input:\n");
    
    keyboard_flush();
    
    /* Wait for Enter key */
    while (1) {
        if (keyboard_has_key()) {
            key_event_t event = keyboard_read_key();
            if (event.pressed && event.ascii == '\n') {
                break;
            }
        }
        __asm__ __volatile__ ("hlt");
    }
    
    /* Display buffer contents */
    terminal_writestring("\nBuffer contents: \"");
    while (keyboard_has_key()) {
        key_event_t event = keyboard_read_key();
        if (event.pressed && event.ascii != 0 && event.ascii != '\n') {
            terminal_putchar(event.ascii);
        }
    }
    terminal_writestring("\"\n");
}

/*
 * Interactive typing test
 */
static void test_typing(void) {
    terminal_writestring("\n=== Typing Test ===\n");
    terminal_writestring("Type the following text:\n");
    terminal_writestring("The quick brown fox jumps over the lazy dog.\n\n");
    terminal_writestring("Your input:\n> ");
    
    keyboard_flush();
    
    const char* target = "The quick brown fox jumps over the lazy dog.";
    int pos = 0;
    int errors = 0;
    
    while (target[pos] != '\0') {
        char ch = keyboard_getchar();
        
        if (ch == 0x08 && pos > 0) {  /* Backspace */
            pos--;
            terminal_writestring("\b \b");
        } else if (ch == target[pos]) {
            terminal_putchar(ch);
            pos++;
        } else if (ch != 0x08) {
            terminal_putchar(ch);
            errors++;
            pos++;
        }
    }
    
    terminal_writestring("\n\nTest complete! Errors: ");
    terminal_write_hex(errors);
    terminal_writestring("\n");
}

/*
 * Run all keyboard tests
 */
void keyboard_run_tests(void) {
    terminal_writestring("\nRunning keyboard driver tests...\n");
    terminal_writestring("===============================\n");
    
    /* Make sure interrupts are enabled */
    __asm__ __volatile__ ("sti");
    
    /* Test 1: Basic character input */
    test_character_input();
    
    /* Test 2: Modifier keys */
    test_modifiers();
    
    /* Test 3: Raw scancodes */
    test_scancodes();
    
    /* Test 4: Buffer test */
    test_buffer();
    
    /* Test 5: Typing test */
    test_typing();
    
    terminal_writestring("\n=== Keyboard Test Summary ===\n");
    terminal_writestring("All keyboard tests completed!\n");
    terminal_writestring("Features tested:\n");
    terminal_writestring("- Character input and display\n");
    terminal_writestring("- Modifier key detection (Shift, Ctrl, Alt, Caps)\n");
    terminal_writestring("- Scancode reading\n");
    terminal_writestring("- Input buffering\n");
    terminal_writestring("- Typing accuracy\n");
    
    /* Disable interrupts again for kernel */
    __asm__ __volatile__ ("cli");
}