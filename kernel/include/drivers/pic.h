/**
 * =============================================================================
 * Chanux OS - Programmable Interrupt Controller (8259A) Driver
 * =============================================================================
 * Controls the Intel 8259A PIC for hardware interrupt management.
 *
 * The system has two PICs (cascaded):
 *   - Master PIC: Handles IRQ 0-7
 *   - Slave PIC:  Handles IRQ 8-15 (connected to master's IRQ 2)
 *
 * By default, the BIOS maps:
 *   - IRQ 0-7 to vectors 0x08-0x0F (conflicts with CPU exceptions!)
 *   - IRQ 8-15 to vectors 0x70-0x77
 *
 * We remap them to vectors 32-47 to avoid conflicts.
 * =============================================================================
 */

#ifndef CHANUX_PIC_H
#define CHANUX_PIC_H

#include "../types.h"

/* =============================================================================
 * PIC I/O Ports
 * =============================================================================
 */

#define PIC1_COMMAND    0x20    /* Master PIC command port */
#define PIC1_DATA       0x21    /* Master PIC data port */
#define PIC2_COMMAND    0xA0    /* Slave PIC command port */
#define PIC2_DATA       0xA1    /* Slave PIC data port */

/* =============================================================================
 * PIC Commands
 * =============================================================================
 */

#define PIC_EOI         0x20    /* End of Interrupt command */

/* ICW1 (Initialization Command Word 1) */
#define ICW1_ICW4       0x01    /* ICW4 needed */
#define ICW1_SINGLE     0x02    /* Single mode (vs cascade) */
#define ICW1_INTERVAL4  0x04    /* Call address interval 4 (vs 8) */
#define ICW1_LEVEL      0x08    /* Level triggered mode (vs edge) */
#define ICW1_INIT       0x10    /* Initialization command */

/* ICW4 (Initialization Command Word 4) */
#define ICW4_8086       0x01    /* 8086/88 mode (vs MCS-80/85) */
#define ICW4_AUTO       0x02    /* Auto EOI mode */
#define ICW4_BUF_SLAVE  0x08    /* Buffered mode (slave) */
#define ICW4_BUF_MASTER 0x0C    /* Buffered mode (master) */
#define ICW4_SFNM       0x10    /* Special fully nested mode */

/* OCW3 (Operation Control Word 3) - for reading ISR/IRR */
#define OCW3_READ_IRR   0x0A    /* Read Interrupt Request Register */
#define OCW3_READ_ISR   0x0B    /* Read In-Service Register */

/* =============================================================================
 * IRQ Vector Mapping
 * =============================================================================
 */

#define PIC1_VECTOR_OFFSET  32  /* Master PIC: IRQ 0-7 -> vectors 32-39 */
#define PIC2_VECTOR_OFFSET  40  /* Slave PIC:  IRQ 8-15 -> vectors 40-47 */

/* =============================================================================
 * PIC Functions
 * =============================================================================
 */

/**
 * Initialize both PICs.
 * Remaps IRQ 0-15 to vectors 32-47.
 */
void pic_init(void);

/**
 * Send End of Interrupt signal to PIC(s).
 * Must be called at the end of every IRQ handler.
 *
 * @param irq IRQ number (0-15)
 */
void pic_send_eoi(uint8_t irq);

/**
 * Mask (disable) an IRQ line.
 *
 * @param irq IRQ number (0-15)
 */
void pic_mask_irq(uint8_t irq);

/**
 * Unmask (enable) an IRQ line.
 *
 * @param irq IRQ number (0-15)
 */
void pic_unmask_irq(uint8_t irq);

/**
 * Get the combined ISR (In-Service Register) value.
 * Shows which interrupts are currently being serviced.
 *
 * @return 16-bit value (low 8 = master, high 8 = slave)
 */
uint16_t pic_get_isr(void);

/**
 * Get the combined IRR (Interrupt Request Register) value.
 * Shows which interrupts are currently pending.
 *
 * @return 16-bit value (low 8 = master, high 8 = slave)
 */
uint16_t pic_get_irr(void);

/**
 * Check if an IRQ is spurious.
 * Spurious IRQs can occur due to electrical noise or timing issues.
 *
 * @param irq IRQ number (0-15)
 * @return true if the IRQ is spurious
 */
bool pic_is_spurious(uint8_t irq);

/**
 * Disable both PICs (mask all IRQs).
 * Used when transitioning to APIC.
 */
void pic_disable(void);

#endif /* CHANUX_PIC_H */
