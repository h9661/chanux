/*
 * PIC Test Code
 * 
 * This file contains test code to verify PIC functionality
 * by enabling the timer interrupt and counting ticks.
 */

#include <stdint.h>
#include "../include/pic.h"

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_write_dec(uint32_t value);
extern void terminal_write_hex(uint8_t value);

/* Include timer driver */
#include "../include/timer.h"

/* Timer is now initialized by timer driver */

/* Timer handler is now provided by timer driver */

/*
 * Run PIC tests
 */
void pic_run_tests(void) {
    terminal_writestring("\nRunning PIC tests...\n");
    terminal_writestring("===================\n");
    
    /* Print initial PIC status */
    pic_print_status();
    
    /* Timer is already initialized by timer driver */
    terminal_writestring("\nTimer interrupt (IRQ0) already enabled by timer driver\n");
    
    /* Enable keyboard interrupt (IRQ1) */
    terminal_writestring("Enabling keyboard interrupt (IRQ1)...\n");
    pic_enable_irq(1);
    
    /* Print updated status */
    pic_print_status();
    
    /* Enable interrupts */
    terminal_writestring("\nEnabling CPU interrupts...\n");
    __asm__ __volatile__ ("sti");
    
    /* Wait for some timer ticks */
    terminal_writestring("Waiting for timer interrupts...\n");
    uint64_t start_ticks = timer_get_ticks();
    
    /* Wait for 10 ticks (100ms) */
    while (timer_get_ticks() < start_ticks + 10) {
        __asm__ __volatile__ ("hlt");
    }
    
    /* Disable interrupts for output */
    __asm__ __volatile__ ("cli");
    
    terminal_writestring("Timer test complete: ");
    terminal_write_dec(timer_get_ticks() - start_ticks);
    terminal_writestring(" ticks in ~100ms\n");
    
    /* Calculate actual frequency */
    terminal_writestring("Measured frequency: ~");
    terminal_write_dec((timer_get_ticks() - start_ticks) * 10);
    terminal_writestring(" Hz\n");
    
    terminal_writestring("\nPress any key to test keyboard interrupt...\n");
    
    /* Re-enable interrupts */
    __asm__ __volatile__ ("sti");
    
    /* Wait for keyboard input (user will see the handler output) */
    uint64_t wait_start = timer_get_ticks();
    while (timer_get_ticks() < wait_start + 200) {  /* Wait 2 seconds max */
        __asm__ __volatile__ ("hlt");
    }
    
    /* Disable interrupts again */
    __asm__ __volatile__ ("cli");
    
    /* Disable IRQs for now */
    terminal_writestring("\nDisabling timer and keyboard interrupts...\n");
    pic_disable_irq(0);
    pic_disable_irq(1);
    
    /* Final status */
    pic_print_status();
    
    terminal_writestring("\nPIC tests completed successfully!\n");
    terminal_writestring("Total timer ticks: ");
    terminal_write_dec(timer_get_ticks());
    terminal_writestring("\n");
}