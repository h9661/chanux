/*
 * Programmable Interrupt Controller (8259A PIC) Implementation
 * 
 * This file implements the PIC initialization and management functions.
 * The PIC is responsible for managing hardware interrupts (IRQs) and
 * delivering them to the CPU in an orderly fashion.
 */

#include <stdint.h>
#include "kernel/pic.h"

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_write_hex(uint8_t value);
extern void terminal_write_dec(uint32_t value);

/* Function declarations */
void pic_print_status(void);

/* External interrupt handlers */
extern void keyboard_interrupt_handler(void);
extern void timer_interrupt_handler(void);

/* Current IRQ mask (1 = disabled, 0 = enabled) */
static uint16_t irq_mask = 0xFFFF;  /* Start with all IRQs disabled */

/*
 * Initialize the PICs and remap IRQs
 * 
 * By default, IRQs 0-7 are mapped to interrupts 0x08-0x0F, which conflicts
 * with CPU exceptions. We remap them to 0x20-0x2F to avoid conflicts.
 */
void pic_init(void) {
    terminal_writestring("Initializing PIC...\n");
    
    /* Save current masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    /* Start initialization sequence (cascade mode) */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    /* ICW2: Set vector offsets */
    outb(PIC1_DATA, IRQ_BASE);      /* Master PIC vector offset (0x20) */
    io_wait();
    outb(PIC2_DATA, IRQ_BASE + 8);  /* Slave PIC vector offset (0x28) */
    io_wait();
    
    /* ICW3: Configure master/slave relationship */
    outb(PIC1_DATA, 0x04);  /* Tell master PIC there's a slave at IRQ2 (0000 0100) */
    io_wait();
    outb(PIC2_DATA, 0x02);  /* Tell slave PIC its cascade identity (0000 0010) */
    io_wait();
    
    /* ICW4: Set mode */
    outb(PIC1_DATA, ICW4_8086);  /* 8086 mode, normal EOI */
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    /* Restore saved masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
    
    /* Update our mask tracking */
    irq_mask = (mask2 << 8) | mask1;
    
    terminal_writestring("PIC initialized: IRQs remapped to 0x");
    terminal_write_hex(IRQ_BASE);
    terminal_writestring("-0x");
    terminal_write_hex(IRQ_BASE + 15);
    terminal_writestring("\n");
}

/*
 * Send End of Interrupt signal to PIC
 * This must be called at the end of an IRQ handler
 */
void pic_send_eoi(uint8_t irq) {
    /* If the IRQ came from the slave PIC, send EOI to both PICs */
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    
    /* Always send EOI to master PIC */
    outb(PIC1_COMMAND, PIC_EOI);
}

/*
 * Enable specific IRQ
 */
void pic_enable_irq(uint8_t irq) {
    uint16_t mask = irq_mask;
    
    /* Clear the bit for this IRQ */
    mask &= ~(1 << irq);
    
    /* Update the PICs */
    pic_set_irq_mask(mask);
}

/*
 * Disable specific IRQ
 */
void pic_disable_irq(uint8_t irq) {
    uint16_t mask = irq_mask;
    
    /* Set the bit for this IRQ */
    mask |= (1 << irq);
    
    /* Update the PICs */
    pic_set_irq_mask(mask);
}

/*
 * Disable all IRQs
 */
void pic_disable_all(void) {
    pic_set_irq_mask(0xFFFF);
}

/*
 * Enable all IRQs
 * Note: This is generally not recommended as some IRQs may not have handlers
 */
void pic_enable_all(void) {
    pic_set_irq_mask(0x0000);
}

/*
 * Get current IRQ mask
 */
uint16_t pic_get_irq_mask(void) {
    return irq_mask;
}

/*
 * Set IRQ mask
 */
void pic_set_irq_mask(uint16_t mask) {
    irq_mask = mask;
    
    /* Update master PIC */
    outb(PIC1_DATA, mask & 0xFF);
    io_wait();
    
    /* Update slave PIC */
    outb(PIC2_DATA, (mask >> 8) & 0xFF);
    io_wait();
}

/*
 * Get Interrupt Request Register
 * Shows which interrupts have been raised but not yet serviced
 */
uint16_t pic_get_irr(void) {
    outb(PIC1_COMMAND, PIC_READ_IRR);
    outb(PIC2_COMMAND, PIC_READ_IRR);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/*
 * Get In-Service Register
 * Shows which interrupts are currently being serviced
 */
uint16_t pic_get_isr(void) {
    outb(PIC1_COMMAND, PIC_READ_ISR);
    outb(PIC2_COMMAND, PIC_READ_ISR);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/*
 * Common IRQ handler stub
 * This function is called by all IRQ handlers
 */
void irq_handler(uint32_t irq_num) {
    /* Handle spurious IRQs */
    if (irq_num == 7) {
        /* Check if this is a spurious IRQ from master PIC */
        uint16_t isr = pic_get_isr();
        if (!(isr & (1 << 7))) {
            return;  /* Spurious, no EOI needed */
        }
    } else if (irq_num == 15) {
        /* Check if this is a spurious IRQ from slave PIC */
        uint16_t isr = pic_get_isr();
        if (!(isr & (1 << 15))) {
            /* Send EOI to master only for cascade IRQ */
            outb(PIC1_COMMAND, PIC_EOI);
            return;
        }
    }
    
    /* Call specific IRQ handlers based on irq_num */
    switch (irq_num) {
        case 0:  /* Timer */
            timer_interrupt_handler();
            break;
        case 1:  /* Keyboard */
            keyboard_interrupt_handler();
            break;
        default:
            /* Unhandled IRQ */
            break;
    }
    
    /* Send EOI */
    pic_send_eoi(irq_num);
}

/*
 * Print PIC status (for debugging)
 */
void pic_print_status(void) {
    terminal_writestring("\nPIC Status:\n");
    
    terminal_writestring("IRQ Mask: 0x");
    terminal_write_hex((irq_mask >> 8) & 0xFF);
    terminal_write_hex(irq_mask & 0xFF);
    terminal_writestring("\n");
    
    uint16_t irr = pic_get_irr();
    terminal_writestring("IRR: 0x");
    terminal_write_hex((irr >> 8) & 0xFF);
    terminal_write_hex(irr & 0xFF);
    terminal_writestring("\n");
    
    uint16_t isr = pic_get_isr();
    terminal_writestring("ISR: 0x");
    terminal_write_hex((isr >> 8) & 0xFF);
    terminal_write_hex(isr & 0xFF);
    terminal_writestring("\n");
    
    /* Show enabled IRQs */
    terminal_writestring("Enabled IRQs: ");
    int first = 1;
    for (int i = 0; i < 16; i++) {
        if (!(irq_mask & (1 << i))) {
            if (!first) terminal_writestring(", ");
            terminal_write_hex(i);
            first = 0;
        }
    }
    if (first) terminal_writestring("None");
    terminal_writestring("\n");
}