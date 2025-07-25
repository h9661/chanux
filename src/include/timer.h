/*
 * Programmable Interval Timer (8253/8254 PIT) Driver
 * 
 * This file defines the interface for the system timer driver.
 * The PIT provides periodic interrupts for timekeeping, scheduling,
 * and delay functions.
 */

#ifndef _TIMER_H
#define _TIMER_H

#include <stdint.h>
#include <stdbool.h>

/* PIT I/O Ports */
#define PIT_CHANNEL0    0x40    /* Channel 0 data port (system timer) */
#define PIT_CHANNEL1    0x41    /* Channel 1 data port (unused) */
#define PIT_CHANNEL2    0x42    /* Channel 2 data port (PC speaker) */
#define PIT_COMMAND     0x43    /* Command/Mode register */

/* PIT Command Byte Format */
#define PIT_CMD_BINARY      0x00    /* Binary counting mode (vs BCD) */
#define PIT_CMD_MODE0       0x00    /* Interrupt on terminal count */
#define PIT_CMD_MODE2       0x04    /* Rate generator */
#define PIT_CMD_MODE3       0x06    /* Square wave generator */
#define PIT_CMD_RW_LSB      0x10    /* Read/Write LSB only */
#define PIT_CMD_RW_MSB      0x20    /* Read/Write MSB only */
#define PIT_CMD_RW_BOTH     0x30    /* Read/Write LSB then MSB */
#define PIT_CMD_COUNTER0    0x00    /* Select counter 0 */
#define PIT_CMD_COUNTER1    0x40    /* Select counter 1 */
#define PIT_CMD_COUNTER2    0x80    /* Select counter 2 */

/* PIT Constants */
#define PIT_BASE_FREQ   1193182     /* Base frequency in Hz (1.193182 MHz) */
#define PIT_IRQ         0           /* Timer uses IRQ0 */

/* Default Timer Settings */
#define TIMER_DEFAULT_FREQ  100     /* Default frequency: 100 Hz (10ms per tick) */

/* Time Units */
#define MS_PER_SEC      1000
#define US_PER_SEC      1000000
#define NS_PER_SEC      1000000000
#define US_PER_MS       1000
#define NS_PER_MS       1000000
#define NS_PER_US       1000

/* Timer Statistics Structure */
typedef struct {
    uint64_t total_ticks;       /* Total ticks since timer init */
    uint32_t frequency;         /* Current timer frequency (Hz) */
    uint32_t ms_per_tick;       /* Milliseconds per tick */
    uint32_t us_per_tick;       /* Microseconds per tick */
    uint64_t uptime_seconds;    /* System uptime in seconds */
    uint64_t uptime_ms;         /* System uptime in milliseconds */
} timer_stats_t;

/* Timer Callback Function Type */
typedef void (*timer_callback_t)(uint64_t tick);

/* Function Declarations */

/* Initialize the PIT timer with given frequency */
void timer_init(uint32_t frequency);

/* Get current tick count */
uint64_t timer_get_ticks(void);

/* Get system uptime in milliseconds */
uint64_t timer_get_uptime_ms(void);

/* Get system uptime in seconds */
uint64_t timer_get_uptime_sec(void);

/* Sleep for specified milliseconds */
void timer_sleep(uint32_t ms);

/* Sleep for specified microseconds (less accurate) */
void timer_usleep(uint32_t us);

/* Busy-wait delay (CPU intensive) */
void timer_delay_ms(uint32_t ms);

/* Get timer frequency */
uint32_t timer_get_frequency(void);

/* Set timer frequency (Hz) */
void timer_set_frequency(uint32_t frequency);

/* Get timer statistics */
timer_stats_t timer_get_stats(void);

/* Register a callback for timer ticks */
void timer_register_callback(timer_callback_t callback);

/* Unregister timer callback */
void timer_unregister_callback(void);

/* Timer interrupt handler (called from IRQ0) */
void timer_interrupt_handler(void);

/* Enable/disable timer interrupt */
void timer_enable(void);
void timer_disable(void);

/* Calculate PIT divisor for given frequency */
uint16_t timer_calculate_divisor(uint32_t frequency);

/* Measure elapsed time (start) */
uint64_t timer_measure_start(void);

/* Measure elapsed time (end) - returns microseconds */
uint64_t timer_measure_end(uint64_t start);

#endif /* _TIMER_H */