/**
 * =============================================================================
 * Chanux OS - Global Descriptor Table Implementation
 * =============================================================================
 * Sets up the GDT with kernel/user segments and TSS for 64-bit long mode.
 *
 * GDT Layout:
 *   Entry 0: Null descriptor (required)
 *   Entry 1: Kernel code segment (0x08) - Ring 0
 *   Entry 2: Kernel data segment (0x10) - Ring 0
 *   Entry 3-4: TSS descriptor (0x18, spans 2 entries in 64-bit mode)
 *   Entry 5: User data segment (0x28) - Ring 3
 *   Entry 6: User code segment (0x30) - Ring 3
 *
 * SYSCALL/SYSRET Requirements:
 *   - STAR MSR bits [63:48] = 0x20 (user segment base - 8)
 *   - SYSRET loads: CS = 0x20 + 16 = 0x30, SS = 0x20 + 8 = 0x28
 *   - With RPL=3: CS = 0x33, SS = 0x2B
 *
 * The TSS provides:
 *   - RSP0 for ring transitions (syscalls, interrupts from Ring 3)
 *   - IST1 stack for double fault handling
 * =============================================================================
 */

#include "../../include/gdt.h"
#include "../../include/kernel.h"
#include "../../drivers/vga/vga.h"

/* =============================================================================
 * Static Data
 * =============================================================================
 */

/* GDT entries - 5 entries (TSS takes 2 slots) */
static gdt_entry_t gdt[GDT_ENTRIES];

/* GDT pointer for LGDT */
static gdt_ptr_t gdtr;

/* Task State Segment */
static tss_t tss;

/* Interrupt Stack for double fault (IST1) */
static uint8_t ist1_stack[IST_STACK_SIZE] ALIGNED(16);

/* =============================================================================
 * Assembly Helpers
 * =============================================================================
 */

/**
 * Load GDT and reload segment registers.
 * We need to reload CS using a far return.
 */
static inline void gdt_load(gdt_ptr_t* gdtr) {
    __asm__ volatile (
        "lgdt (%0)\n"           /* Load GDT */
        /* Reload CS using far return */
        "pushq $0x08\n"         /* Push code segment selector */
        "leaq 1f(%%rip), %%rax\n" /* Load address of label 1 */
        "pushq %%rax\n"         /* Push return address */
        "lretq\n"               /* Far return to reload CS */
        "1:\n"
        /* Reload data segment registers */
        "movw $0x10, %%ax\n"    /* Data segment selector */
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        :
        : "r"(gdtr)
        : "rax", "memory"
    );
}

/**
 * Load TSS into TR register.
 */
static inline void tss_load(uint16_t selector) {
    __asm__ volatile ("ltr %0" : : "r"(selector));
}

/* =============================================================================
 * GDT Entry Setup Helpers
 * =============================================================================
 */

/**
 * Set a regular GDT entry (null, code, or data segment).
 */
static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity) {
    gdt[index].limit_low    = (uint16_t)(limit & 0xFFFF);
    gdt[index].base_low     = (uint16_t)(base & 0xFFFF);
    gdt[index].base_mid     = (uint8_t)((base >> 16) & 0xFF);
    gdt[index].access       = access;
    gdt[index].granularity  = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
    gdt[index].base_high    = (uint8_t)((base >> 24) & 0xFF);
}

/**
 * Set up the TSS descriptor (spans 2 GDT entries in 64-bit mode).
 */
static void gdt_set_tss(int index, uint64_t base, uint32_t limit) {
    /* First 8 bytes (low part of TSS descriptor) */
    tss_descriptor_t* tss_desc = (tss_descriptor_t*)&gdt[index];

    tss_desc->limit_low     = (uint16_t)(limit & 0xFFFF);
    tss_desc->base_low      = (uint16_t)(base & 0xFFFF);
    tss_desc->base_mid_low  = (uint8_t)((base >> 16) & 0xFF);
    tss_desc->access        = GDT_ACCESS_TSS;  /* 0x89 = 64-bit TSS, available */
    tss_desc->granularity   = (uint8_t)((limit >> 16) & 0x0F);
    tss_desc->base_mid_high = (uint8_t)((base >> 24) & 0xFF);
    tss_desc->base_high     = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    tss_desc->reserved      = 0;
}

/* =============================================================================
 * TSS Initialization
 * =============================================================================
 */

/**
 * Initialize the Task State Segment.
 */
static void tss_init(void) {
    /* Clear the TSS */
    uint8_t* tss_ptr = (uint8_t*)&tss;
    for (size_t i = 0; i < sizeof(tss_t); i++) {
        tss_ptr[i] = 0;
    }

    /* Set up IST1 for double fault handling */
    /* Stack grows downward, so point to the top of the stack */
    tss.ist1 = (uint64_t)&ist1_stack[IST_STACK_SIZE];

    /* RSP0 will be set when we have proper kernel stack management */
    /* For now, it's 0 (we're always in ring 0) */
    tss.rsp0 = 0;

    /* I/O Map Base - point past the TSS to disable I/O permission bitmap */
    tss.iomap_base = sizeof(tss_t);
}

/* =============================================================================
 * GDT Initialization
 * =============================================================================
 */

/**
 * Initialize the Global Descriptor Table with TSS.
 */
void gdt_init(void) {
    /* Initialize TSS first */
    tss_init();

    /* Entry 0: Null descriptor (required by x86) */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Entry 1: Kernel code segment (selector 0x08) */
    /* 64-bit code segment: executable, readable, present, ring 0 */
    gdt_set_entry(1, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL0 | GDT_ACCESS_SEGMENT |
                  GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
                  GDT_GRAN_LONG_MODE | GDT_GRAN_4K);

    /* Entry 2: Kernel data segment (selector 0x10) */
    /* Data segment: writable, present, ring 0 */
    gdt_set_entry(2, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL0 | GDT_ACCESS_SEGMENT |
                  GDT_ACCESS_RW,
                  GDT_GRAN_4K);

    /* Entry 3-4: TSS descriptor (selector 0x18, spans 2 entries) */
    gdt_set_tss(3, (uint64_t)&tss, sizeof(tss_t) - 1);

    /* Entry 5: User data segment (selector 0x28, with RPL 0x2B) */
    /* Data segment: writable, present, ring 3 */
    gdt_set_entry(5, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 | GDT_ACCESS_SEGMENT |
                  GDT_ACCESS_RW,
                  GDT_GRAN_4K);

    /* Entry 6: User code segment (selector 0x30, with RPL 0x33) */
    /* 64-bit code segment: executable, readable, present, ring 3 */
    gdt_set_entry(6, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 | GDT_ACCESS_SEGMENT |
                  GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
                  GDT_GRAN_LONG_MODE | GDT_GRAN_4K);

    /* Set up GDT pointer */
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt;

    /* Load GDT and reload segment registers */
    gdt_load(&gdtr);

    /* Load TSS */
    tss_load(GDT_TSS_SEL);
}

/* =============================================================================
 * TSS Access Functions
 * =============================================================================
 */

/**
 * Get the current RSP0 value from TSS.
 */
uint64_t gdt_get_rsp0(void) {
    return tss.rsp0;
}

/**
 * Set the RSP0 value in TSS.
 */
void gdt_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
