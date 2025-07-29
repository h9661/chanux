/*
 * Programmable Interval Timer (8253/8254 PIT) Driver Implementation
 * 
 * This driver manages the system timer, providing timekeeping,
 * delays, and periodic interrupts for scheduling.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "kernel/timer.h"
#include "kernel/pic.h"

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_write_dec(uint32_t value);

/* Timer state */
static volatile uint64_t timer_ticks = 0;
static uint32_t timer_frequency = TIMER_DEFAULT_FREQ;
static uint32_t timer_ms_per_tick = 1000 / TIMER_DEFAULT_FREQ;
static uint32_t timer_us_per_tick = 1000000 / TIMER_DEFAULT_FREQ;
static timer_callback_t timer_callback = NULL;
static bool timer_enabled = false;

/*
 * Calculate PIT divisor for desired frequency
 */
uint16_t timer_calculate_divisor(uint32_t frequency) {
    if (frequency == 0) {
        frequency = TIMER_DEFAULT_FREQ;
    }
    
    /* Ensure frequency is within valid range */
    if (frequency > PIT_BASE_FREQ) {
        frequency = PIT_BASE_FREQ;
    } else if (frequency < 19) {  /* Minimum ~18.2 Hz */
        frequency = 19;
    }
    
    return (uint16_t)(PIT_BASE_FREQ / frequency);
}

/*
 * Set PIT frequency
 */
void timer_set_frequency(uint32_t frequency) {
    uint16_t divisor = timer_calculate_divisor(frequency);
    
    /* Update state */
    timer_frequency = PIT_BASE_FREQ / divisor;  /* Actual frequency */
    timer_ms_per_tick = 1000 / timer_frequency;
    timer_us_per_tick = 1000000 / timer_frequency;
    
    /* Send command byte */
    outb(PIT_COMMAND, PIT_CMD_BINARY | PIT_CMD_MODE3 | 
                      PIT_CMD_RW_BOTH | PIT_CMD_COUNTER0);
    
    /* Send divisor (LSB then MSB) */
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

/*
 * Initialize timer with given frequency
 */
void timer_init(uint32_t frequency) {
    terminal_writestring("Initializing timer at ");
    terminal_write_dec(frequency);
    terminal_writestring(" Hz...\n");
    
    /* Reset state */
    timer_ticks = 0;
    timer_callback = NULL;
    
    /* Set frequency */
    timer_set_frequency(frequency);
    
    /* Enable timer interrupt (IRQ0) */
    timer_enable();
    
    terminal_writestring("Timer initialized: ");
    terminal_write_dec(timer_ms_per_tick);
    terminal_writestring(" ms per tick\n");
}

/*
 * Timer interrupt handler
 */
void timer_interrupt_handler(void) {
    /* Increment tick counter */
    timer_ticks++;

    /* Call registered callback if any */
    if (timer_callback) {
        timer_callback(timer_ticks);
    }
    
    /* Call scheduler on every timer tick */
    extern void scheduler_tick(void);
    scheduler_tick();
}

/*
 * Enable timer interrupt
 */
void timer_enable(void) {
    if (!timer_enabled) {
        pic_enable_irq(PIT_IRQ);
        timer_enabled = true;
    }
}

/*
 * Disable timer interrupt
 */
void timer_disable(void) {
    if (timer_enabled) {
        pic_disable_irq(PIT_IRQ);
        timer_enabled = false;
    }
}

/*
 * Get current tick count
 */
uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

/*
 * Get uptime in milliseconds
 */
uint64_t timer_get_uptime_ms(void) {
    return timer_ticks * timer_ms_per_tick;
}

/*
 * Get uptime in seconds
 */
uint64_t timer_get_uptime_sec(void) {
    /* Simple version: convert to milliseconds first, then to seconds */
    /* This avoids 64-bit division */
    uint64_t ms = timer_get_uptime_ms();
    
    /* Convert milliseconds to seconds using 32-bit division */
    uint32_t ms_low = (uint32_t)(ms & 0xFFFFFFFF);
    uint32_t ms_high = (uint32_t)(ms >> 32);
    
    if (ms_high == 0) {
        /* Common case: less than ~49 days uptime */
        return ms_low / 1000;
    }
    
    /* For very long uptimes, just return an approximation */
    return (uint64_t)(ms_low / 1000) + ((uint64_t)ms_high * 4294967);  /* 2^32 / 1000 */
}

/*
 * Get timer frequency
 */
uint32_t timer_get_frequency(void) {
    return timer_frequency;
}

/*
 * Sleep for specified milliseconds
 */
void timer_sleep(uint32_t ms) {
    if (ms == 0) return;
    
    /* Use scheduler's process_sleep for proper sleep implementation */
    extern void process_sleep(uint32_t ms);
    process_sleep(ms);
}

/*
 * Sleep for microseconds (less accurate)
 */
void timer_usleep(uint32_t us) {
    if (us == 0) return;
    
    /* Convert to milliseconds if large enough */
    if (us >= 1000) {
        timer_sleep(us / 1000);
        return;
    }
    
    /* For small delays, busy wait */
    uint64_t start = timer_get_ticks();
    uint64_t ticks_to_wait = (us + timer_us_per_tick - 1) / timer_us_per_tick;
    if (ticks_to_wait == 0) ticks_to_wait = 1;
    
    while ((timer_get_ticks() - start) < ticks_to_wait) {
        /* Busy wait */
        __asm__ __volatile__ ("pause");
    }
}

/*
 * Busy-wait delay in milliseconds
 */
void timer_delay_ms(uint32_t ms) {
    if (ms == 0) return;
    
    uint64_t start = timer_get_ticks();
    uint64_t ticks_to_wait = (ms + timer_ms_per_tick - 1) / timer_ms_per_tick;
    
    while ((timer_get_ticks() - start) < ticks_to_wait) {
        /* Busy wait */
        __asm__ __volatile__ ("pause");
    }
}

/*
 * Get timer statistics
 */
timer_stats_t timer_get_stats(void) {
    timer_stats_t stats;
    
    stats.total_ticks = timer_ticks;
    stats.frequency = timer_frequency;
    stats.ms_per_tick = timer_ms_per_tick;
    stats.us_per_tick = timer_us_per_tick;
    stats.uptime_seconds = timer_get_uptime_sec();
    stats.uptime_ms = timer_get_uptime_ms();
    
    return stats;
}

/*
 * Register timer callback
 */
void timer_register_callback(timer_callback_t callback) {
    timer_callback = callback;
}

/*
 * Unregister timer callback
 */
void timer_unregister_callback(void) {
    timer_callback = NULL;
}

/*
 * Start time measurement
 */
uint64_t timer_measure_start(void) {
    return timer_ticks;
}

/*
 * End time measurement (returns microseconds)
 */
uint64_t timer_measure_end(uint64_t start) {
    uint64_t elapsed_ticks = timer_ticks - start;
    return elapsed_ticks * timer_us_per_tick;
}