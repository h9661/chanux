/*
 * Task State Segment (TSS) Structure
 * 
 * The TSS is used by x86 processors to store information about tasks.
 * In our implementation, we use a single TSS for switching privilege levels.
 */

#ifndef TSS_H
#define TSS_H

#include <stdint.h>

/* Task State Segment structure */
typedef struct {
    uint32_t prev_tss;    /* Previous TSS selector (not used) */
    uint32_t esp0;        /* Stack pointer for ring 0 (kernel) */
    uint32_t ss0;         /* Stack segment for ring 0 */
    uint32_t esp1;        /* Stack pointer for ring 1 (not used) */
    uint32_t ss1;         /* Stack segment for ring 1 */
    uint32_t esp2;        /* Stack pointer for ring 2 (not used) */
    uint32_t ss2;         /* Stack segment for ring 2 */
    uint32_t cr3;         /* Page directory base address */
    uint32_t eip;         /* Instruction pointer */
    uint32_t eflags;      /* CPU flags */
    uint32_t eax;         /* General purpose registers */
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;         /* Stack pointer */
    uint32_t ebp;         /* Base pointer */
    uint32_t esi;         /* Source index */
    uint32_t edi;         /* Destination index */
    uint32_t es;          /* Segment selectors */
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;         /* Local descriptor table (not used) */
    uint16_t trap;        /* Trap on task switch flag */
    uint16_t iomap_base;  /* I/O map base address */
} __attribute__((packed)) tss_t;

/* Function declarations */
void tss_init(void);
void tss_set_kernel_stack(uint32_t stack);

#endif /* TSS_H */