/*
 * Task State Segment (TSS) Implementation
 * 
 * Manages the TSS for privilege level switching between kernel and user mode.
 */

#include "arch/x86/tss.h"
#include <stdint.h>
#include "lib/string.h"

/* External functions */
extern void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
extern void tss_flush(void);

/* Global TSS instance */
static tss_t tss;

/*
 * tss_init - Initialize the Task State Segment
 * 
 * Sets up the TSS with default values and installs it in the GDT.
 * The TSS is primarily used for storing the kernel stack pointer
 * when switching from user mode to kernel mode.
 */
void tss_init(void) {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(tss_t) - 1;
    
    /* Clear the TSS structure */
    memset(&tss, 0, sizeof(tss_t));
    
    /* Set up the TSS with kernel segments */
    tss.ss0 = 0x10;    /* Kernel data segment selector */
    tss.esp0 = 0;      /* Will be set when switching tasks */
    
    /* Set up segment selectors for kernel mode */
    tss.cs = 0x08 | 0x3;    /* Code segment with RPL 3 */
    tss.ss = 0x10 | 0x3;    /* Data segments with RPL 3 */
    tss.ds = 0x10 | 0x3;
    tss.es = 0x10 | 0x3;
    tss.fs = 0x10 | 0x3;
    tss.gs = 0x10 | 0x3;
    
    /* Set I/O map base to beyond TSS limit (no I/O permissions) */
    tss.iomap_base = sizeof(tss_t);
    
    /* Install TSS descriptor in GDT slot 5
     * Access byte: 0x89 (present, DPL=0, 32-bit TSS available)
     * Granularity: 0x00 (byte granularity)
     */
    gdt_set_gate(5, base, limit, 0x89, 0x00);
    
    /* Load the TSS selector (0x28 = 5 << 3) */
    tss_flush();
}

/*
 * tss_set_kernel_stack - Update kernel stack pointer in TSS
 * @stack: New kernel stack pointer
 * 
 * This function updates the ESP0 field in the TSS, which is used
 * by the CPU when switching from user mode to kernel mode.
 */
void tss_set_kernel_stack(uint32_t stack) {
    tss.esp0 = stack;
}