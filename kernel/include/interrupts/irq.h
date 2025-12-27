/**
 * =============================================================================
 * Chanux OS - Hardware IRQ Handler Framework
 * =============================================================================
 * Provides IRQ handler registration and dispatch for hardware interrupts.
 *
 * Hardware IRQs (0-15) are mapped to vectors 32-47 after PIC remapping.
 * Device drivers register their handlers here, and the common irq_handler()
 * dispatches to the appropriate registered handler.
 * =============================================================================
 */

#ifndef CHANUX_IRQ_H
#define CHANUX_IRQ_H

#include "isr.h"

/* =============================================================================
 * IRQ Constants
 * =============================================================================
 */

#define IRQ_COUNT           16      /* Total number of IRQs (0-15) */

/* =============================================================================
 * IRQ Functions
 * =============================================================================
 */

/**
 * Initialize the IRQ framework.
 * Clears all registered handlers.
 */
void irq_init(void);

/**
 * Register a handler for a hardware IRQ.
 *
 * @param irq     IRQ number (0-15)
 * @param handler Handler function to call when IRQ fires
 */
void irq_register_handler(uint8_t irq, isr_handler_t handler);

/**
 * Unregister a handler for a hardware IRQ.
 *
 * @param irq IRQ number (0-15)
 */
void irq_unregister_handler(uint8_t irq);

/**
 * Common IRQ handler (called from assembly stubs).
 * Dispatches to registered handlers and sends EOI.
 *
 * @param regs Pointer to saved register state
 */
void irq_handler(registers_t* regs);

#endif /* CHANUX_IRQ_H */
