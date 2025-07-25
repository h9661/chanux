/*
 * Programmable Interrupt Controller (8259A PIC)
 * 
 * This file defines the interface for the 8259A PIC chip.
 * The PIC manages hardware interrupts (IRQs) and maps them
 * to interrupt vectors that the CPU can handle.
 * 
 * The PC has two PICs in a master-slave configuration:
 * - Master PIC: Handles IRQ 0-7
 * - Slave PIC: Handles IRQ 8-15
 */

#ifndef _PIC_H
#define _PIC_H

#include <stdint.h>

/* PIC I/O Port Addresses */
#define PIC1_COMMAND    0x20    /* Master PIC command port */
#define PIC1_DATA       0x21    /* Master PIC data port */
#define PIC2_COMMAND    0xA0    /* Slave PIC command port */
#define PIC2_DATA       0xA1    /* Slave PIC data port */

/* PIC Commands */
#define PIC_EOI         0x20    /* End of Interrupt command */
#define PIC_READ_IRR    0x0A    /* Read Interrupt Request Register */
#define PIC_READ_ISR    0x0B    /* Read In-Service Register */

/* ICW1 - Initialization Command Word 1 */
#define ICW1_ICW4       0x01    /* ICW4 needed */
#define ICW1_SINGLE     0x02    /* Single (not cascade) mode */
#define ICW1_INTERVAL4  0x04    /* Call address interval 4 (not 8) */
#define ICW1_LEVEL      0x08    /* Level triggered (not edge) mode */
#define ICW1_INIT       0x10    /* Initialization bit */

/* ICW4 - Initialization Command Word 4 */
#define ICW4_8086       0x01    /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02    /* Auto (not normal) EOI */
#define ICW4_BUF_SLAVE  0x08    /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C    /* Buffered mode/master */
#define ICW4_SFNM       0x10    /* Special fully nested mode */

/* IRQ Numbers */
#define IRQ0    0       /* Programmable Interval Timer */
#define IRQ1    1       /* Keyboard */
#define IRQ2    2       /* Cascade for slave PIC */
#define IRQ3    3       /* COM2 */
#define IRQ4    4       /* COM1 */
#define IRQ5    5       /* LPT2 */
#define IRQ6    6       /* Floppy disk */
#define IRQ7    7       /* LPT1 */
#define IRQ8    8       /* Real-time clock */
#define IRQ9    9       /* Available */
#define IRQ10   10      /* Available */
#define IRQ11   11      /* Available */
#define IRQ12   12      /* PS/2 Mouse */
#define IRQ13   13      /* FPU/Coprocessor */
#define IRQ14   14      /* Primary IDE */
#define IRQ15   15      /* Secondary IDE */

/* IRQ to Interrupt Vector Mapping */
#define IRQ_BASE        0x20    /* Base interrupt vector for IRQs */
#define IRQ0_VECTOR     (IRQ_BASE + 0)
#define IRQ1_VECTOR     (IRQ_BASE + 1)
#define IRQ2_VECTOR     (IRQ_BASE + 2)
#define IRQ3_VECTOR     (IRQ_BASE + 3)
#define IRQ4_VECTOR     (IRQ_BASE + 4)
#define IRQ5_VECTOR     (IRQ_BASE + 5)
#define IRQ6_VECTOR     (IRQ_BASE + 6)
#define IRQ7_VECTOR     (IRQ_BASE + 7)
#define IRQ8_VECTOR     (IRQ_BASE + 8)
#define IRQ9_VECTOR     (IRQ_BASE + 9)
#define IRQ10_VECTOR    (IRQ_BASE + 10)
#define IRQ11_VECTOR    (IRQ_BASE + 11)
#define IRQ12_VECTOR    (IRQ_BASE + 12)
#define IRQ13_VECTOR    (IRQ_BASE + 13)
#define IRQ14_VECTOR    (IRQ_BASE + 14)
#define IRQ15_VECTOR    (IRQ_BASE + 15)

/* Function Declarations */

/* Initialize the PICs and remap IRQs to avoid conflicts with CPU exceptions */
void pic_init(void);

/* Send End of Interrupt signal to PIC */
void pic_send_eoi(uint8_t irq);

/* Enable/disable specific IRQ */
void pic_enable_irq(uint8_t irq);
void pic_disable_irq(uint8_t irq);

/* Mask/unmask all IRQs */
void pic_disable_all(void);
void pic_enable_all(void);

/* Get current IRQ mask */
uint16_t pic_get_irq_mask(void);

/* Set IRQ mask */
void pic_set_irq_mask(uint16_t mask);

/* Get IRQ register values (for debugging) */
uint16_t pic_get_irr(void);  /* Interrupt Request Register */
uint16_t pic_get_isr(void);  /* In-Service Register */

/* Helper functions for port I/O */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(value), "d"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(value) : "d"(port));
    return value;
}

/* Wait for I/O operation to complete */
static inline void io_wait(void) {
    /* Port 0x80 is used for 'checkpoints' during POST */
    outb(0x80, 0);
}

#endif /* _PIC_H */