/**
 * =============================================================================
 * Chanux OS - IRQ Handler Framework Implementation
 * =============================================================================
 * Dispatches hardware IRQs to registered device driver handlers.
 *
 * When an IRQ fires:
 *   1. Assembly stub saves registers and calls irq_handler()
 *   2. irq_handler() checks for spurious IRQ
 *   3. irq_handler() dispatches to registered handler (if any)
 *   4. irq_handler() sends EOI to PIC
 *   5. Assembly stub restores registers and returns
 * =============================================================================
 */

#include "../include/interrupts/irq.h"
#include "../include/interrupts/idt.h"
#include "../include/drivers/pic.h"
#include "../drivers/vga/vga.h"

/* =============================================================================
 * Static Data
 * =============================================================================
 */

/* Table of registered IRQ handlers */
static isr_handler_t irq_handlers[IRQ_COUNT];

/* =============================================================================
 * IRQ Framework Initialization
 * =============================================================================
 */

/**
 * Initialize the IRQ framework.
 */
void irq_init(void) {
    /* Clear all handler pointers */
    for (int i = 0; i < IRQ_COUNT; i++) {
        irq_handlers[i] = NULL;
    }
}

/* =============================================================================
 * Handler Registration
 * =============================================================================
 */

/**
 * Register a handler for a hardware IRQ.
 */
void irq_register_handler(uint8_t irq, isr_handler_t handler) {
    if (irq < IRQ_COUNT) {
        irq_handlers[irq] = handler;
    }
}

/**
 * Unregister a handler for a hardware IRQ.
 */
void irq_unregister_handler(uint8_t irq) {
    if (irq < IRQ_COUNT) {
        irq_handlers[irq] = NULL;
    }
}

/* =============================================================================
 * Common IRQ Handler
 * =============================================================================
 */

/**
 * Common IRQ handler called from assembly stubs.
 */
void irq_handler(registers_t* regs) {
    /* Calculate IRQ number from interrupt vector */
    uint8_t irq = (uint8_t)(regs->int_no - IRQ_VECTOR_BASE);

    /* Validate IRQ number */
    if (irq >= IRQ_COUNT) {
        /* Invalid IRQ, just send EOI and return */
        pic_send_eoi(irq);
        return;
    }

    /* Check for spurious IRQ */
    if (pic_is_spurious(irq)) {
        /* Spurious IRQ7 - don't send EOI to master */
        /* Spurious IRQ15 - send EOI to master only (for cascade) */
        if (irq >= 8) {
            pic_send_eoi(2);  /* EOI to master for cascade line */
        }
        return;
    }

    /* Call registered handler if present */
    if (irq_handlers[irq] != NULL) {
        irq_handlers[irq](regs);
    }

    /* Send End of Interrupt to PIC */
    pic_send_eoi(irq);
}
