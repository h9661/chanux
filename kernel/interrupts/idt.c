/**
 * =============================================================================
 * Chanux OS - Interrupt Descriptor Table Implementation
 * =============================================================================
 * Sets up the 256-entry IDT for 64-bit long mode.
 *
 * The IDT is initialized with:
 *   - Exception handlers for vectors 0-31
 *   - IRQ handlers for vectors 32-47
 *   - Empty entries for vectors 48-255 (available for software interrupts)
 * =============================================================================
 */

#include "../include/interrupts/idt.h"
#include "../include/interrupts/isr.h"
#include "../drivers/vga/vga.h"

/* =============================================================================
 * Static Data
 * =============================================================================
 */

/* The actual IDT - 256 entries, 16 bytes each = 4KB */
static idt_entry_t idt[IDT_ENTRIES];

/* IDT pointer for LIDT instruction */
static idt_ptr_t idtr;

/* =============================================================================
 * IDT Entry Setup
 * =============================================================================
 */

/**
 * Set an IDT entry.
 * Configures a single IDT gate with the specified handler and attributes.
 */
void idt_set_entry(uint8_t vector, uint64_t handler,
                   uint16_t selector, uint8_t type_attr, uint8_t ist) {
    idt_entry_t* entry = &idt[vector];

    /* Split the 64-bit handler address into the three fields */
    entry->offset_low  = (uint16_t)(handler & 0xFFFF);
    entry->offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    entry->offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);

    /* Set the code segment selector */
    entry->selector = selector;

    /* Set IST (Interrupt Stack Table) index - only bits 0-2 are used */
    entry->ist = ist & 0x07;

    /* Set type and attributes */
    entry->type_attr = type_attr;

    /* Reserved field must be zero */
    entry->reserved = 0;
}

/* =============================================================================
 * IDT Initialization
 * =============================================================================
 */

/**
 * Initialize the Interrupt Descriptor Table.
 * Sets up all exception and IRQ handlers, then loads the IDTR.
 */
void idt_init(void) {
    /* Clear the entire IDT first */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].reserved = 0;
    }

    /* Set up exception handlers (vectors 0-31) */
    /* Using the ISR stub table from idt.asm */
    idt_set_entry(0,  (uint64_t)isr0,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(1,  (uint64_t)isr1,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(2,  (uint64_t)isr2,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(3,  (uint64_t)isr3,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(4,  (uint64_t)isr4,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(5,  (uint64_t)isr5,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(6,  (uint64_t)isr6,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(7,  (uint64_t)isr7,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(8,  (uint64_t)isr8,  KERNEL_CS, IDT_GATE_INTERRUPT, 1);  /* Double fault uses IST1 */
    idt_set_entry(9,  (uint64_t)isr9,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(10, (uint64_t)isr10, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(11, (uint64_t)isr11, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(12, (uint64_t)isr12, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(13, (uint64_t)isr13, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(14, (uint64_t)isr14, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(15, (uint64_t)isr15, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(16, (uint64_t)isr16, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(17, (uint64_t)isr17, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(18, (uint64_t)isr18, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(19, (uint64_t)isr19, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(20, (uint64_t)isr20, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(21, (uint64_t)isr21, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(22, (uint64_t)isr22, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(23, (uint64_t)isr23, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(24, (uint64_t)isr24, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(25, (uint64_t)isr25, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(26, (uint64_t)isr26, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(27, (uint64_t)isr27, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(28, (uint64_t)isr28, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(29, (uint64_t)isr29, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(30, (uint64_t)isr30, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(31, (uint64_t)isr31, KERNEL_CS, IDT_GATE_INTERRUPT, 0);

    /* Set up IRQ handlers (vectors 32-47) */
    idt_set_entry(32, (uint64_t)irq0,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(33, (uint64_t)irq1,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(34, (uint64_t)irq2,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(35, (uint64_t)irq3,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(36, (uint64_t)irq4,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(37, (uint64_t)irq5,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(38, (uint64_t)irq6,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(39, (uint64_t)irq7,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(40, (uint64_t)irq8,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(41, (uint64_t)irq9,  KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(42, (uint64_t)irq10, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(43, (uint64_t)irq11, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(44, (uint64_t)irq12, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(45, (uint64_t)irq13, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(46, (uint64_t)irq14, KERNEL_CS, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(47, (uint64_t)irq15, KERNEL_CS, IDT_GATE_INTERRUPT, 0);

    /* Set up the IDT pointer */
    idtr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idtr.base = (uint64_t)&idt;

    /* Load the IDT */
    idt_load(&idtr);
}
