/**
 * =============================================================================
 * Chanux OS - PIC (8259A) Driver Implementation
 * =============================================================================
 * Initializes and controls the Intel 8259A Programmable Interrupt Controller.
 *
 * Initialization sequence:
 *   1. Send ICW1 to both PICs (starts initialization)
 *   2. Send ICW2 to both PICs (set vector offsets)
 *   3. Send ICW3 to both PICs (set cascade configuration)
 *   4. Send ICW4 to both PICs (set operating mode)
 *   5. Mask all interrupts initially
 * =============================================================================
 */

#include "../../include/drivers/pic.h"
#include "../../include/kernel.h"

/* =============================================================================
 * PIC Initialization
 * =============================================================================
 */

/**
 * Initialize both PICs with IRQ remapping.
 */
void pic_init(void) {
    /* Save current masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* ICW1: Start initialization sequence (cascade mode, ICW4 needed) */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: Set vector offsets */
    outb(PIC1_DATA, PIC1_VECTOR_OFFSET);    /* Master: IRQ 0-7 -> vectors 32-39 */
    io_wait();
    outb(PIC2_DATA, PIC2_VECTOR_OFFSET);    /* Slave: IRQ 8-15 -> vectors 40-47 */
    io_wait();

    /* ICW3: Configure cascade */
    outb(PIC1_DATA, 0x04);  /* Master: Slave on IRQ2 (bit 2) */
    io_wait();
    outb(PIC2_DATA, 0x02);  /* Slave: Cascade identity (IRQ2) */
    io_wait();

    /* ICW4: Set 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restore saved masks (or mask all initially) */
    /* For now, mask all IRQs except cascade (IRQ2) */
    outb(PIC1_DATA, 0xFB);  /* Mask all except IRQ2 (cascade) */
    outb(PIC2_DATA, 0xFF);  /* Mask all slave IRQs */

    /* Alternative: restore original masks */
    /* outb(PIC1_DATA, mask1); */
    /* outb(PIC2_DATA, mask2); */
    (void)mask1;
    (void)mask2;
}

/* =============================================================================
 * End of Interrupt
 * =============================================================================
 */

/**
 * Send End of Interrupt to PIC(s).
 * Must be called at the end of every IRQ handler.
 */
void pic_send_eoi(uint8_t irq) {
    /* If IRQ came from slave PIC, send EOI to both */
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    /* Always send EOI to master */
    outb(PIC1_COMMAND, PIC_EOI);
}

/* =============================================================================
 * IRQ Masking
 * =============================================================================
 */

/**
 * Mask (disable) an IRQ line.
 */
void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) | (1 << irq);
    outb(port, value);
}

/**
 * Unmask (enable) an IRQ line.
 */
void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

/* =============================================================================
 * ISR/IRR Reading
 * =============================================================================
 */

/**
 * Get the In-Service Register value.
 */
uint16_t pic_get_isr(void) {
    outb(PIC1_COMMAND, OCW3_READ_ISR);
    outb(PIC2_COMMAND, OCW3_READ_ISR);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/**
 * Get the Interrupt Request Register value.
 */
uint16_t pic_get_irr(void) {
    outb(PIC1_COMMAND, OCW3_READ_IRR);
    outb(PIC2_COMMAND, OCW3_READ_IRR);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/* =============================================================================
 * Spurious IRQ Detection
 * =============================================================================
 */

/**
 * Check if an IRQ is spurious.
 *
 * Spurious IRQs can occur on IRQ7 (master) and IRQ15 (slave) due to:
 *   - Electrical noise on the IRQ line
 *   - IRQ line going low before the CPU acknowledges
 *
 * To detect: Check if the corresponding ISR bit is set.
 * If not set, it's spurious.
 */
bool pic_is_spurious(uint8_t irq) {
    /* Only IRQ7 and IRQ15 can be spurious */
    if (irq == 7) {
        outb(PIC1_COMMAND, OCW3_READ_ISR);
        return (inb(PIC1_COMMAND) & 0x80) == 0;
    } else if (irq == 15) {
        outb(PIC2_COMMAND, OCW3_READ_ISR);
        return (inb(PIC2_COMMAND) & 0x80) == 0;
    }
    return false;
}

/* =============================================================================
 * PIC Disable
 * =============================================================================
 */

/**
 * Disable both PICs by masking all IRQs.
 * Used when transitioning to APIC.
 */
void pic_disable(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
