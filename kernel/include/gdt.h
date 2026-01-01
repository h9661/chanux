/**
 * =============================================================================
 * Chanux OS - Global Descriptor Table
 * =============================================================================
 * Defines the GDT structures for 64-bit long mode with TSS support.
 *
 * In 64-bit mode, segmentation is mostly disabled, but we still need:
 *   - Null descriptor (required)
 *   - Kernel code segment (for CS)
 *   - Kernel data segment (for SS, DS, ES, FS, GS)
 *   - TSS descriptor (for task state and IST)
 *
 * The TSS is required for:
 *   - Interrupt Stack Table (IST) for certain exceptions
 *   - Ring transitions (RSP0 for syscalls in future phases)
 * =============================================================================
 */

#ifndef CHANUX_GDT_H
#define CHANUX_GDT_H

#include "types.h"

/* =============================================================================
 * GDT Constants
 * =============================================================================
 */

/* Segment selectors (byte offsets into GDT) */
#define GDT_NULL_SEL        0x00    /* Null descriptor */
#define GDT_KERNEL_CODE     0x08    /* Kernel code segment */
#define GDT_KERNEL_DATA     0x10    /* Kernel data segment */
#define GDT_TSS_SEL         0x18    /* TSS descriptor (16 bytes in 64-bit) */
#define GDT_USER_DATA       0x28    /* User data segment (Ring 3) */
#define GDT_USER_CODE       0x30    /* User code segment (Ring 3) */

/* Selectors with RPL (Requested Privilege Level) for user mode */
#define GDT_USER_DATA_RPL   0x2B    /* 0x28 | RPL 3 */
#define GDT_USER_CODE_RPL   0x33    /* 0x30 | RPL 3 */

/* Number of regular GDT entries (TSS takes 2 slots in 64-bit mode) */
#define GDT_ENTRIES         7       /* null, kcode, kdata, tss_low, tss_high, udata, ucode */

/* =============================================================================
 * GDT Entry Structure (8 bytes)
 * =============================================================================
 */

typedef struct {
    uint16_t limit_low;         /* Limit bits 0-15 */
    uint16_t base_low;          /* Base bits 0-15 */
    uint8_t  base_mid;          /* Base bits 16-23 */
    uint8_t  access;            /* Access byte */
    uint8_t  granularity;       /* Granularity and limit bits 16-19 */
    uint8_t  base_high;         /* Base bits 24-31 */
} PACKED gdt_entry_t;

/* =============================================================================
 * GDT Pointer Structure
 * =============================================================================
 * Used with LGDT instruction.
 */

typedef struct {
    uint16_t limit;             /* Size of GDT - 1 */
    uint64_t base;              /* Linear address of GDT */
} PACKED gdt_ptr_t;

/* =============================================================================
 * TSS Structure (Task State Segment for 64-bit mode)
 * =============================================================================
 * In 64-bit mode, the TSS is used for:
 *   - RSP values for privilege level transitions
 *   - IST (Interrupt Stack Table) for handling certain interrupts
 *
 * See Intel SDM Vol. 3A, Section 7.7
 */

typedef struct {
    uint32_t reserved0;         /* Reserved */
    uint64_t rsp0;              /* Stack pointer for ring 0 */
    uint64_t rsp1;              /* Stack pointer for ring 1 */
    uint64_t rsp2;              /* Stack pointer for ring 2 */
    uint64_t reserved1;         /* Reserved */
    uint64_t ist1;              /* Interrupt Stack Table 1 (double fault) */
    uint64_t ist2;              /* Interrupt Stack Table 2 */
    uint64_t ist3;              /* Interrupt Stack Table 3 */
    uint64_t ist4;              /* Interrupt Stack Table 4 */
    uint64_t ist5;              /* Interrupt Stack Table 5 */
    uint64_t ist6;              /* Interrupt Stack Table 6 */
    uint64_t ist7;              /* Interrupt Stack Table 7 */
    uint64_t reserved2;         /* Reserved */
    uint16_t reserved3;         /* Reserved */
    uint16_t iomap_base;        /* I/O Map Base Address */
} PACKED tss_t;

/* =============================================================================
 * TSS Descriptor (16 bytes in 64-bit mode)
 * =============================================================================
 * The TSS descriptor spans two GDT slots in 64-bit mode because it needs
 * to hold a 64-bit base address.
 */

typedef struct {
    uint16_t limit_low;         /* Limit bits 0-15 */
    uint16_t base_low;          /* Base bits 0-15 */
    uint8_t  base_mid_low;      /* Base bits 16-23 */
    uint8_t  access;            /* Access byte */
    uint8_t  granularity;       /* Granularity and limit bits 16-19 */
    uint8_t  base_mid_high;     /* Base bits 24-31 */
    uint32_t base_high;         /* Base bits 32-63 */
    uint32_t reserved;          /* Must be zero */
} PACKED tss_descriptor_t;

/* =============================================================================
 * GDT Access Byte Flags
 * =============================================================================
 */

#define GDT_ACCESS_PRESENT      0x80    /* Segment present */
#define GDT_ACCESS_DPL0         0x00    /* Ring 0 */
#define GDT_ACCESS_DPL3         0x60    /* Ring 3 */
#define GDT_ACCESS_SEGMENT      0x10    /* Descriptor type (code/data) */
#define GDT_ACCESS_EXECUTABLE   0x08    /* Executable (code segment) */
#define GDT_ACCESS_RW           0x02    /* Readable (code) / Writable (data) */
#define GDT_ACCESS_ACCESSED     0x01    /* Accessed */
#define GDT_ACCESS_TSS          0x89    /* 64-bit TSS (available) */

/* =============================================================================
 * GDT Granularity Flags
 * =============================================================================
 */

#define GDT_GRAN_LONG_MODE      0x20    /* 64-bit code segment */
#define GDT_GRAN_4K             0x80    /* 4KB granularity */

/* =============================================================================
 * IST Configuration
 * =============================================================================
 */

#define IST_STACK_SIZE          4096    /* 4KB stack for each IST entry */

/* =============================================================================
 * GDT Functions
 * =============================================================================
 */

/**
 * Initialize the Global Descriptor Table with TSS.
 * Creates GDT entries and loads GDTR, then loads TSS.
 */
void gdt_init(void);

/**
 * Get the kernel stack pointer for ring 0 transitions.
 * Used when switching from user mode to kernel mode.
 *
 * @return Current RSP0 value from TSS
 */
uint64_t gdt_get_rsp0(void);

/**
 * Set the kernel stack pointer for ring 0 transitions.
 * Called during task switching to update the kernel stack.
 *
 * @param rsp0 New RSP0 value for TSS
 */
void gdt_set_rsp0(uint64_t rsp0);

#endif /* CHANUX_GDT_H */
