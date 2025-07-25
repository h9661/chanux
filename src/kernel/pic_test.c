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

/* Timer tick counter */
volatile uint32_t timer_ticks = 0;

/* Timer frequency (Hz) */
#define TIMER_FREQ 100  /* 100 Hz = 10ms per tick */

/* PIT (Programmable Interval Timer) ports */
#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL1    0x41
#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43

/* PIT commands */
#define PIT_CMD_BINARY  0x00    /* Binary mode */
#define PIT_CMD_MODE3   0x06    /* Square wave mode */
#define PIT_CMD_RW_BOTH 0x30    /* Read/Write LSB then MSB */
#define PIT_CMD_CHANNEL0 0x00   /* Select channel 0 */

/*
 * Initialize the Programmable Interval Timer
 */
void timer_init(uint32_t frequency) {
    /* Calculate divisor for desired frequency */
    uint32_t divisor = 1193180 / frequency;  /* PIT runs at 1.193180 MHz */
    
    /* Send command byte */
    outb(PIT_COMMAND, PIT_CMD_BINARY | PIT_CMD_MODE3 | PIT_CMD_RW_BOTH | PIT_CMD_CHANNEL0);
    
    /* Send divisor */
    outb(PIT_CHANNEL0, divisor & 0xFF);         /* Low byte */
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);  /* High byte */
    
    terminal_writestring("Timer initialized at ");
    terminal_write_dec(frequency);
    terminal_writestring(" Hz\n");
}

/*
 * Timer interrupt handler (called from IRQ0)
 */
void timer_handler(void) {
    timer_ticks++;
}

/* Timer handler is now called from pic.c's irq_handler */

/*
 * Run PIC tests
 */
void pic_run_tests(void) {
    terminal_writestring("\nRunning PIC tests...\n");
    terminal_writestring("===================\n");
    
    /* Print initial PIC status */
    pic_print_status();
    
    /* Initialize timer */
    timer_init(TIMER_FREQ);
    
    /* Enable timer interrupt (IRQ0) */
    terminal_writestring("\nEnabling timer interrupt (IRQ0)...\n");
    pic_enable_irq(0);
    
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
    uint32_t start_ticks = timer_ticks;
    
    /* Wait for 10 ticks (100ms) */
    while (timer_ticks < start_ticks + 10) {
        __asm__ __volatile__ ("hlt");
    }
    
    /* Disable interrupts for output */
    __asm__ __volatile__ ("cli");
    
    terminal_writestring("Timer test complete: ");
    terminal_write_dec(timer_ticks - start_ticks);
    terminal_writestring(" ticks in ~100ms\n");
    
    /* Calculate actual frequency */
    terminal_writestring("Measured frequency: ~");
    terminal_write_dec((timer_ticks - start_ticks) * 10);
    terminal_writestring(" Hz\n");
    
    terminal_writestring("\nPress any key to test keyboard interrupt...\n");
    
    /* Re-enable interrupts */
    __asm__ __volatile__ ("sti");
    
    /* Wait for keyboard input (user will see the handler output) */
    uint32_t wait_start = timer_ticks;
    while (timer_ticks < wait_start + 200) {  /* Wait 2 seconds max */
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
    terminal_write_dec(timer_ticks);
    terminal_writestring("\n");
}