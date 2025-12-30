/**
 * =============================================================================
 * Chanux OS - PIT (8254) Driver Implementation
 * =============================================================================
 * Configures the PIT for system timing at 100Hz.
 *
 * The timer interrupt handler:
 *   1. Increments the tick counter
 *   2. (Future) Triggers scheduler tick
 *   3. Returns to allow EOI and iret
 * =============================================================================
 */

#include "../../include/drivers/pit.h"
#include "../../include/drivers/pic.h"
#include "../../include/interrupts/irq.h"
#include "../../include/interrupts/idt.h"
#include "../../include/proc/sched.h"
#include "../../include/kernel.h"
#include "../vga/vga.h"

/* =============================================================================
 * Static Data
 * =============================================================================
 */

/* Tick counter - incremented every timer interrupt */
static volatile uint64_t tick_count = 0;

/* =============================================================================
 * Timer Interrupt Handler
 * =============================================================================
 */

/**
 * PIT IRQ0 handler.
 * Called 100 times per second.
 */
static void pit_irq_handler(registers_t* regs) {
    /* Increment tick counter */
    tick_count++;

    /* Call scheduler tick handler for preemption */
    if (sched_is_running()) {
        sched_tick(regs);
    }
}

/* =============================================================================
 * PIT Initialization
 * =============================================================================
 */

/**
 * Initialize the PIT.
 */
void pit_init(void) {
    /* Calculate divisor for desired frequency */
    uint16_t divisor = PIT_DIVISOR;

    /* Send command byte:
     *   - Channel 0
     *   - Access mode: low byte then high byte
     *   - Mode 3: square wave generator
     *   - Binary mode
     */
    outb(PIT_COMMAND, PIT_CMD_CHANNEL0 | PIT_CMD_LOHI | PIT_CMD_MODE3 | PIT_CMD_BINARY);

    /* Send divisor (low byte first, then high byte) */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    io_wait();
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register IRQ0 handler */
    irq_register_handler(0, pit_irq_handler);

    /* Enable IRQ0 */
    pic_unmask_irq(0);
}

/* =============================================================================
 * Timing Functions
 * =============================================================================
 */

/**
 * Get current tick count.
 */
uint64_t pit_get_ticks(void) {
    return tick_count;
}

/**
 * Get uptime in milliseconds.
 */
uint64_t pit_get_uptime_ms(void) {
    return tick_count * PIT_MS_PER_TICK;
}

/**
 * Get uptime in seconds.
 */
uint64_t pit_get_uptime_sec(void) {
    return tick_count / PIT_TICK_RATE;
}

/**
 * Sleep for specified number of ticks.
 */
void pit_sleep_ticks(uint64_t ticks) {
    uint64_t target = tick_count + ticks;
    while (tick_count < target) {
        halt();  /* Wait for interrupt */
    }
}

/**
 * Sleep for specified number of milliseconds.
 */
void pit_sleep_ms(uint64_t ms) {
    /* Convert ms to ticks (round up to ensure minimum delay) */
    uint64_t ticks = (ms + PIT_MS_PER_TICK - 1) / PIT_MS_PER_TICK;
    pit_sleep_ticks(ticks);
}
