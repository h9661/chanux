/**
 * =============================================================================
 * Chanux OS - Interrupt Descriptor Table
 * =============================================================================
 * Defines the IDT structures and functions for 64-bit long mode.
 *
 * The IDT contains 256 entries, each describing how to handle an interrupt:
 *   - Entries 0-31: CPU exceptions (divide error, page fault, etc.)
 *   - Entries 32-47: Hardware IRQs (after PIC remapping)
 *   - Entries 48-255: Available for software interrupts
 * =============================================================================
 */

#ifndef CHANUX_IDT_H
#define CHANUX_IDT_H

#include "../types.h"

/* =============================================================================
 * IDT Constants
 * =============================================================================
 */

#define IDT_ENTRIES         256

/* IDT Gate Types (for type_attr field) */
#define IDT_GATE_INTERRUPT  0x8E    /* Present, DPL=0, 64-bit Interrupt Gate */
#define IDT_GATE_TRAP       0x8F    /* Present, DPL=0, 64-bit Trap Gate */
#define IDT_GATE_USER       0xEE    /* Present, DPL=3, 64-bit Interrupt Gate */

/* Code segment selector */
#define KERNEL_CS           0x08

/* =============================================================================
 * Exception Vectors (0-31)
 * =============================================================================
 */

#define EXCEPTION_DE        0       /* Divide Error */
#define EXCEPTION_DB        1       /* Debug */
#define EXCEPTION_NMI       2       /* Non-Maskable Interrupt */
#define EXCEPTION_BP        3       /* Breakpoint */
#define EXCEPTION_OF        4       /* Overflow */
#define EXCEPTION_BR        5       /* Bound Range Exceeded */
#define EXCEPTION_UD        6       /* Invalid Opcode */
#define EXCEPTION_NM        7       /* Device Not Available */
#define EXCEPTION_DF        8       /* Double Fault */
#define EXCEPTION_CSO       9       /* Coprocessor Segment Overrun (reserved) */
#define EXCEPTION_TS        10      /* Invalid TSS */
#define EXCEPTION_NP        11      /* Segment Not Present */
#define EXCEPTION_SS        12      /* Stack Segment Fault */
#define EXCEPTION_GP        13      /* General Protection Fault */
#define EXCEPTION_PF        14      /* Page Fault */
#define EXCEPTION_RESERVED  15      /* Reserved */
#define EXCEPTION_MF        16      /* x87 FPU Floating-Point Error */
#define EXCEPTION_AC        17      /* Alignment Check */
#define EXCEPTION_MC        18      /* Machine Check */
#define EXCEPTION_XM        19      /* SIMD Floating-Point Exception */
#define EXCEPTION_VE        20      /* Virtualization Exception */
#define EXCEPTION_CP        21      /* Control Protection Exception */
/* 22-31 are reserved */

/* =============================================================================
 * IRQ Vectors (after PIC remapping)
 * =============================================================================
 */

#define IRQ_VECTOR_BASE     32      /* IRQs start at vector 32 */

#define IRQ0                32      /* PIT Timer */
#define IRQ1                33      /* Keyboard */
#define IRQ2                34      /* Cascade (slave PIC) */
#define IRQ3                35      /* COM2 */
#define IRQ4                36      /* COM1 */
#define IRQ5                37      /* LPT2 */
#define IRQ6                38      /* Floppy Disk */
#define IRQ7                39      /* LPT1 / Spurious */
#define IRQ8                40      /* CMOS RTC */
#define IRQ9                41      /* ACPI / Legacy SCSI / NIC */
#define IRQ10               42      /* Open */
#define IRQ11               43      /* Open */
#define IRQ12               44      /* PS/2 Mouse */
#define IRQ13               45      /* FPU / Coprocessor */
#define IRQ14               46      /* Primary ATA */
#define IRQ15               47      /* Secondary ATA */

/* =============================================================================
 * IDT Entry Structure (64-bit mode)
 * =============================================================================
 * Each IDT entry is 16 bytes in 64-bit mode.
 * See Intel SDM Vol. 3A, Section 6.14.1
 */

typedef struct {
    uint16_t offset_low;        /* Offset bits 0-15 */
    uint16_t selector;          /* Code segment selector */
    uint8_t  ist;               /* Interrupt Stack Table (bits 0-2), rest reserved */
    uint8_t  type_attr;         /* Type and attributes */
    uint16_t offset_mid;        /* Offset bits 16-31 */
    uint32_t offset_high;       /* Offset bits 32-63 */
    uint32_t reserved;          /* Must be zero */
} PACKED idt_entry_t;

/* =============================================================================
 * IDT Pointer Structure
 * =============================================================================
 * Used with LIDT instruction to load the IDT.
 */

typedef struct {
    uint16_t limit;             /* Size of IDT - 1 */
    uint64_t base;              /* Virtual address of IDT */
} PACKED idt_ptr_t;

/* =============================================================================
 * IDT Functions
 * =============================================================================
 */

/**
 * Initialize the Interrupt Descriptor Table.
 * Sets up all 256 entries and loads the IDTR.
 */
void idt_init(void);

/**
 * Set an IDT entry.
 *
 * @param vector    Interrupt vector number (0-255)
 * @param handler   Handler function address
 * @param selector  Code segment selector (usually KERNEL_CS)
 * @param type_attr Type and attributes (IDT_GATE_*)
 * @param ist       Interrupt Stack Table index (0 for none, 1-7 for IST)
 */
void idt_set_entry(uint8_t vector, uint64_t handler,
                   uint16_t selector, uint8_t type_attr, uint8_t ist);

/**
 * Load the IDT using LIDT instruction.
 * Implemented in assembly (idt.asm).
 *
 * @param idtr Pointer to IDT pointer structure
 */
extern void idt_load(idt_ptr_t* idtr);

#endif /* CHANUX_IDT_H */
