/*
 * Timer Driver Test
 * 
 * This file contains test code for the timer driver,
 * demonstrating frequency control, time measurement,
 * delays, and callbacks.
 */

#include <stdint.h>
#include <stdbool.h>
#include "../include/timer.h"

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_write_dec(uint32_t value);
extern void terminal_putchar(char c);

/* Test state */
static uint32_t callback_count = 0;
static uint64_t last_callback_tick = 0;

/*
 * Timer callback for testing
 */
static void test_timer_callback(uint64_t tick) {
    callback_count++;
    last_callback_tick = tick;
}

/*
 * Test basic timer functionality
 */
static void test_basic_timer(void) {
    terminal_writestring("\n=== Basic Timer Test ===\n");
    
    /* Get initial stats */
    timer_stats_t stats = timer_get_stats();
    terminal_writestring("Timer frequency: ");
    terminal_write_dec(stats.frequency);
    terminal_writestring(" Hz\n");
    terminal_writestring("Milliseconds per tick: ");
    terminal_write_dec(stats.ms_per_tick);
    terminal_writestring("\n");
    terminal_writestring("Microseconds per tick: ");
    terminal_write_dec(stats.us_per_tick);
    terminal_writestring("\n");
    
    /* Test tick counting */
    terminal_writestring("\nCounting 10 ticks...\n");
    uint64_t start_tick = timer_get_ticks();
    
    for (int i = 0; i < 10; i++) {
        uint64_t current = timer_get_ticks();
        terminal_writestring("Tick ");
        terminal_write_dec(i + 1);
        terminal_writestring(": ");
        terminal_write_dec(current);
        terminal_writestring("\n");
        
        /* Wait for next tick */
        while (timer_get_ticks() == current) {
            __asm__ __volatile__ ("hlt");
        }
    }
    
    uint64_t elapsed = timer_get_ticks() - start_tick;
    terminal_writestring("Elapsed ticks: ");
    terminal_write_dec(elapsed);
    terminal_writestring("\n");
}

/*
 * Test sleep functionality
 */
static void test_sleep(void) {
    terminal_writestring("\n=== Sleep Test ===\n");
    
    /* Test various sleep durations */
    uint32_t sleep_times[] = {100, 250, 500, 1000};
    
    for (int i = 0; i < 4; i++) {
        terminal_writestring("Sleeping for ");
        terminal_write_dec(sleep_times[i]);
        terminal_writestring(" ms...");
        
        uint64_t start = timer_get_uptime_ms();
        timer_sleep(sleep_times[i]);
        uint64_t elapsed = timer_get_uptime_ms() - start;
        
        terminal_writestring(" done! (actual: ");
        terminal_write_dec(elapsed);
        terminal_writestring(" ms)\n");
    }
}

/*
 * Test time measurement
 */
static void test_measurement(void) {
    terminal_writestring("\n=== Time Measurement Test ===\n");
    
    /* Measure various operations */
    terminal_writestring("Measuring operation times...\n");
    
    /* Measure empty loop */
    uint64_t start = timer_measure_start();
    for (volatile int i = 0; i < 10000; i++) {
        /* Empty loop */
    }
    uint64_t elapsed = timer_measure_end(start);
    terminal_writestring("10,000 iterations: ");
    terminal_write_dec(elapsed);
    terminal_writestring(" microseconds\n");
    
    /* Measure string output */
    start = timer_measure_start();
    terminal_writestring("Test string output timing...\n");
    elapsed = timer_measure_end(start);
    terminal_writestring("String output: ");
    terminal_write_dec(elapsed);
    terminal_writestring(" microseconds\n");
    
    /* Measure delay accuracy */
    terminal_writestring("\nTesting delay accuracy:\n");
    start = timer_measure_start();
    timer_delay_ms(100);
    elapsed = timer_measure_end(start);
    terminal_writestring("100ms delay measured as: ");
    terminal_write_dec(elapsed);
    terminal_writestring(" microseconds\n");
}

/*
 * Test timer callbacks
 */
static void test_callbacks(void) {
    terminal_writestring("\n=== Callback Test ===\n");
    
    /* Reset callback state */
    callback_count = 0;
    last_callback_tick = 0;
    
    /* Register callback */
    terminal_writestring("Registering timer callback...\n");
    timer_register_callback(test_timer_callback);
    
    /* Wait for some callbacks */
    terminal_writestring("Waiting for 50 timer callbacks...\n");
    while (callback_count < 50) {
        __asm__ __volatile__ ("hlt");
    }
    
    terminal_writestring("Callbacks received: ");
    terminal_write_dec(callback_count);
    terminal_writestring("\n");
    terminal_writestring("Last callback at tick: ");
    terminal_write_dec(last_callback_tick);
    terminal_writestring("\n");
    
    /* Unregister callback */
    timer_unregister_callback();
    terminal_writestring("Callback unregistered\n");
}

/*
 * Test frequency changes
 */
static void test_frequency_change(void) {
    terminal_writestring("\n=== Frequency Change Test ===\n");
    
    uint32_t frequencies[] = {50, 100, 200, 1000};
    
    for (int i = 0; i < 4; i++) {
        terminal_writestring("\nChanging frequency to ");
        terminal_write_dec(frequencies[i]);
        terminal_writestring(" Hz\n");
        
        timer_set_frequency(frequencies[i]);
        
        /* Measure actual frequency */
        uint64_t start_tick = timer_get_ticks();
        uint64_t start_ms = timer_get_uptime_ms();
        
        /* Wait approximately 1 second */
        timer_sleep(1000);
        
        uint64_t tick_diff = timer_get_ticks() - start_tick;
        uint64_t ms_diff = timer_get_uptime_ms() - start_ms;
        
        terminal_writestring("Ticks in ~1 second: ");
        terminal_write_dec(tick_diff);
        terminal_writestring(" (expected ~");
        terminal_write_dec(frequencies[i]);
        terminal_writestring(")\n");
        
        terminal_writestring("Actual time elapsed: ");
        terminal_write_dec(ms_diff);
        terminal_writestring(" ms\n");
    }
    
    /* Restore default frequency */
    terminal_writestring("\nRestoring default frequency (");
    terminal_write_dec(TIMER_DEFAULT_FREQ);
    terminal_writestring(" Hz)\n");
    timer_set_frequency(TIMER_DEFAULT_FREQ);
}

/*
 * Test uptime tracking
 */
static void test_uptime(void) {
    terminal_writestring("\n=== Uptime Test ===\n");
    
    /* Display current uptime */
    timer_stats_t stats = timer_get_stats();
    terminal_writestring("System uptime: ");
    terminal_write_dec(stats.uptime_seconds);
    terminal_writestring(" seconds (");
    terminal_write_dec(stats.uptime_ms);
    terminal_writestring(" ms)\n");
    terminal_writestring("Total ticks: ");
    terminal_write_dec(stats.total_ticks);
    terminal_writestring("\n");
    
    /* Show live uptime for a few seconds */
    terminal_writestring("\nLive uptime display (5 seconds):\n");
    uint64_t end_time = timer_get_uptime_ms() + 5000;
    
    while (timer_get_uptime_ms() < end_time) {
        uint64_t uptime = timer_get_uptime_sec();
        terminal_writestring("\rUptime: ");
        terminal_write_dec(uptime);
        terminal_writestring(" seconds    ");
        
        timer_sleep(100);  /* Update every 100ms */
    }
    terminal_writestring("\n");
}

/*
 * Run all timer tests
 */
void timer_run_tests(void) {
    terminal_writestring("\nRunning timer driver tests...\n");
    terminal_writestring("============================\n");
    
    /* Ensure interrupts are enabled */
    __asm__ __volatile__ ("sti");
    
    /* Test 1: Basic timer functionality */
    test_basic_timer();
    
    /* Test 2: Sleep functions */
    test_sleep();
    
    /* Test 3: Time measurement */
    test_measurement();
    
    /* Test 4: Timer callbacks */
    test_callbacks();
    
    /* Test 5: Frequency changes */
    test_frequency_change();
    
    /* Test 6: Uptime tracking */
    test_uptime();
    
    terminal_writestring("\n=== Timer Test Summary ===\n");
    terminal_writestring("All timer tests completed!\n");
    terminal_writestring("Features tested:\n");
    terminal_writestring("- Tick counting and frequency control\n");
    terminal_writestring("- Sleep and delay functions\n");
    terminal_writestring("- Time measurement\n");
    terminal_writestring("- Timer callbacks\n");
    terminal_writestring("- Frequency changes\n");
    terminal_writestring("- Uptime tracking\n");
    
    /* Display final statistics */
    timer_stats_t final_stats = timer_get_stats();
    terminal_writestring("\nFinal timer statistics:\n");
    terminal_writestring("Total test duration: ");
    terminal_write_dec(final_stats.uptime_seconds);
    terminal_writestring(" seconds\n");
    terminal_writestring("Total ticks: ");
    terminal_write_dec(final_stats.total_ticks);
    terminal_writestring("\n");
    
    /* Disable interrupts for kernel */
    __asm__ __volatile__ ("cli");
}