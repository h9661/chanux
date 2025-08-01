/*
 * Extended Process Structure for User Mode Support
 * 
 * This file extends the basic process structure with memory management
 * fields required for user mode support and security.
 */

#ifndef _PROCESS_H
#define _PROCESS_H

#include <stdint.h>
#include "kernel/scheduler.h"
#include "kernel/memory_protection.h"

/* Signal definitions */
#define SIGKILL     9   /* Kill signal */
#define SIGSEGV    11   /* Segmentation violation */

/* Extended process structure with memory management */
typedef struct process_extended {
    process_t base;                 /* Base process structure */
    
    /* Extended memory management */
    uint32_t user_stack_size;       /* Size of user stack */
    uint32_t heap_start;            /* Start of heap */
    uint32_t heap_end;              /* Current heap end (brk) */
    
    /* Memory protection */
    bool kernel_only;               /* Process runs in kernel mode only */
    uint32_t memory_limit;          /* Maximum memory usage */
    
} process_extended_t;

/* Function prototypes */
process_t* get_current_process(void);
void process_terminate(process_t* proc, int signal);

/* Helper to get extended process structure */
static inline process_extended_t* get_process_extended(process_t* proc) {
    return (process_extended_t*)proc;
}

#endif /* _PROCESS_H */