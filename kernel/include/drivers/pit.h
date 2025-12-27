/**
 * =============================================================================
 * Chanux OS - Programmable Interval Timer (8254) Driver
 * =============================================================================
 * Controls the Intel 8254 PIT for system timing.
 *
 * The PIT has 3 channels:
 *   - Channel 0: Connected to IRQ0, used for system timer
 *   - Channel 1: Historically for DRAM refresh (not used)
 *   - Channel 2: Connected to PC speaker
 *
 * We configure Channel 0 to generate interrupts at 100Hz (10ms intervals).
 * =============================================================================
 */

#ifndef CHANUX_PIT_H
#define CHANUX_PIT_H

#include "../types.h"

/* =============================================================================
 * PIT I/O Ports
 * =============================================================================
 */

#define PIT_CHANNEL0    0x40    /* Channel 0 data port */
#define PIT_CHANNEL1    0x41    /* Channel 1 data port */
#define PIT_CHANNEL2    0x42    /* Channel 2 data port */
#define PIT_COMMAND     0x43    /* Command register */

/* =============================================================================
 * PIT Command Register Bits
 * =============================================================================
 */

/* Channel select (bits 6-7) */
#define PIT_CMD_CHANNEL0    0x00    /* Select channel 0 */
#define PIT_CMD_CHANNEL1    0x40    /* Select channel 1 */
#define PIT_CMD_CHANNEL2    0x80    /* Select channel 2 */
#define PIT_CMD_READBACK    0xC0    /* Read-back command */

/* Access mode (bits 4-5) */
#define PIT_CMD_LATCH       0x00    /* Latch count value */
#define PIT_CMD_LOBYTE      0x10    /* Access low byte only */
#define PIT_CMD_HIBYTE      0x20    /* Access high byte only */
#define PIT_CMD_LOHI        0x30    /* Access low byte then high byte */

/* Operating mode (bits 1-3) */
#define PIT_CMD_MODE0       0x00    /* Interrupt on terminal count */
#define PIT_CMD_MODE1       0x02    /* Hardware retriggerable one-shot */
#define PIT_CMD_MODE2       0x04    /* Rate generator */
#define PIT_CMD_MODE3       0x06    /* Square wave generator */
#define PIT_CMD_MODE4       0x08    /* Software triggered strobe */
#define PIT_CMD_MODE5       0x0A    /* Hardware triggered strobe */

/* BCD/Binary mode (bit 0) */
#define PIT_CMD_BINARY      0x00    /* 16-bit binary counter */
#define PIT_CMD_BCD         0x01    /* 4-digit BCD counter */

/* =============================================================================
 * PIT Configuration
 * =============================================================================
 */

#define PIT_BASE_FREQ       1193182     /* Base oscillator frequency in Hz */
#define PIT_TICK_RATE       100         /* Desired tick rate in Hz */
#define PIT_DIVISOR         (PIT_BASE_FREQ / PIT_TICK_RATE)  /* ~11932 */
#define PIT_MS_PER_TICK     (1000 / PIT_TICK_RATE)           /* 10ms */

/* =============================================================================
 * PIT Functions
 * =============================================================================
 */

/**
 * Initialize the Programmable Interval Timer.
 * Configures channel 0 for periodic interrupts at PIT_TICK_RATE Hz.
 */
void pit_init(void);

/**
 * Get the current tick count since boot.
 *
 * @return Number of timer ticks since PIT initialization
 */
uint64_t pit_get_ticks(void);

/**
 * Get system uptime in milliseconds.
 *
 * @return Milliseconds since PIT initialization
 */
uint64_t pit_get_uptime_ms(void);

/**
 * Get system uptime in seconds.
 *
 * @return Seconds since PIT initialization
 */
uint64_t pit_get_uptime_sec(void);

/**
 * Sleep for a specified number of ticks.
 * Busy-waits until the tick count is reached.
 *
 * @param ticks Number of ticks to wait
 */
void pit_sleep_ticks(uint64_t ticks);

/**
 * Sleep for a specified number of milliseconds.
 * Busy-waits until the time has elapsed.
 *
 * @param ms Milliseconds to sleep
 */
void pit_sleep_ms(uint64_t ms);

#endif /* CHANUX_PIT_H */
