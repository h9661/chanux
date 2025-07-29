/*
 * Process Scheduler Implementation
 * 
 * This implements a basic round-robin scheduler for ChanUX.
 * The scheduler manages process creation, switching, and termination.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include "kernel/paging.h"

/* Process states */
typedef enum {
    PROCESS_STATE_READY,      /* Process is ready to run */
    PROCESS_STATE_RUNNING,    /* Process is currently running */
    PROCESS_STATE_BLOCKED,    /* Process is waiting for I/O or event */
    PROCESS_STATE_TERMINATED  /* Process has finished execution */
} process_state_t;

/* Maximum number of processes */
#define MAX_PROCESSES 64
#define PROCESS_STACK_SIZE 4096  /* 4KB stack per process */

/* Process ID type */
typedef uint32_t pid_t;

/* CPU context structure - saved during context switch */
typedef struct {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} __attribute__((packed)) cpu_context_t;

/* Process Control Block (PCB) */
typedef struct process {
    pid_t pid;                      /* Process ID */
    char name[32];                  /* Process name */
    process_state_t state;          /* Current process state */
    
    /* CPU context */
    cpu_context_t context;          /* Saved CPU registers */
    uint32_t kernel_stack;          /* Kernel stack pointer */
    uint32_t user_stack;            /* User stack pointer */
    
    /* Memory management */
    page_directory_t* page_directory;        /* Pointer to page directory */
    
    /* Scheduling information */
    uint32_t time_slice;            /* Remaining time slice */
    uint32_t priority;              /* Process priority (not used in round-robin) */
    struct process *next;           /* Next process in ready queue */
    uint64_t wake_time;             /* Time to wake up (for sleeping processes) */
    
    /* Process hierarchy */
    pid_t parent_pid;               /* Parent process ID */
    
    /* Statistics */
    uint64_t cpu_time;              /* Total CPU time used */
    uint64_t start_time;            /* Process start time */
} process_t;

/* Scheduler statistics */
typedef struct {
    uint32_t context_switches;      /* Total number of context switches */
    uint32_t processes_created;     /* Total processes created */
    uint32_t processes_terminated;  /* Total processes terminated */
} scheduler_stats_t;

/* Function declarations */
void scheduler_init(void);
pid_t process_create(const char *name, void (*entry_point)(void));
void process_exit(int status);
void process_yield(void);
void process_sleep(uint32_t ms);
void process_wake(pid_t pid);
process_t* process_get_current(void);
pid_t process_get_current_pid(void);
void scheduler_tick(void);
scheduler_stats_t scheduler_get_stats(void);

/* Assembly functions for context switching */
extern void switch_context(cpu_context_t *old, cpu_context_t *new);
extern void process_start(void);

#endif /* SCHEDULER_H */