/**
 * =============================================================================
 * Chanux OS - Process Management
 * =============================================================================
 * Defines the Process Control Block (PCB) structure and process management API.
 *
 * Phase 4 implements kernel-mode processes with:
 *   - Process creation and termination
 *   - Process state management
 *   - Context switching support
 *   - Integration with the scheduler
 *
 * All processes run in Ring 0 (kernel mode) in this phase.
 * User mode (Ring 3) will be added in Phase 5.
 * =============================================================================
 */

#ifndef CHANUX_PROCESS_H
#define CHANUX_PROCESS_H

#include "../types.h"
#include "../interrupts/isr.h"

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

#define MAX_PROCESSES       64      /* Maximum concurrent processes */
#define PROCESS_NAME_MAX    32      /* Maximum process name length */
#define KERNEL_STACK_SIZE   8192    /* 8KB kernel stack per process */
#define DEFAULT_TIME_SLICE  10      /* Time slice in ticks (100ms at 100Hz) */

/* =============================================================================
 * Process States
 * =============================================================================
 */

typedef enum {
    PROCESS_STATE_UNUSED = 0,       /* PCB slot is free */
    PROCESS_STATE_READY,            /* Ready to run, in run queue */
    PROCESS_STATE_RUNNING,          /* Currently executing on CPU */
    PROCESS_STATE_BLOCKED,          /* Waiting for I/O or event */
    PROCESS_STATE_TERMINATED        /* Finished execution, awaiting cleanup */
} process_state_t;

/* =============================================================================
 * Process Flags
 * =============================================================================
 */

#define PROCESS_FLAG_KERNEL     0x01    /* Kernel process (Ring 0) */
#define PROCESS_FLAG_IDLE       0x02    /* System idle process */
#define PROCESS_FLAG_USER       0x04    /* User process (Ring 3) */

/* =============================================================================
 * Process Control Block (PCB)
 * =============================================================================
 * The PCB contains all information needed to manage a process.
 *
 * Memory layout for kernel stack:
 *   kernel_stack      -> Base of allocated memory (low address)
 *   kernel_stack_top  -> Top of stack (high address, 16-byte aligned)
 *   rsp              -> Current stack pointer (context saved here)
 */

typedef struct process {
    /* === Identification === */
    pid_t               pid;                        /* Process ID (unique) */
    char                name[PROCESS_NAME_MAX];     /* Process name for debugging */

    /* === State === */
    process_state_t     state;                      /* Current process state */
    uint32_t            flags;                      /* Process flags */

    /* === Stack Management === */
    void*               kernel_stack;               /* Base of kernel stack (allocated) */
    uint64_t            kernel_stack_top;           /* Top of kernel stack (RSP0) */
    uint64_t            rsp;                        /* Saved stack pointer (context) */

    /* === Entry Point (for new processes) === */
    void                (*entry)(void*);            /* Entry point function */
    void*               entry_arg;                  /* Argument to entry point */

    /* === Scheduling === */
    uint32_t            time_slice;                 /* Ticks remaining in quantum */
    uint32_t            priority;                   /* Priority (for future use) */
    uint64_t            total_ticks;                /* Total CPU ticks consumed */

    /* === Linked List Pointers === */
    struct process*     next;                       /* Next in list (run queue) */
    struct process*     prev;                       /* Previous in list */

    /* === Exit Information === */
    int                 exit_code;                  /* Exit code (for terminated) */

    /* === Parent/Child (for future fork support) === */
    pid_t               parent_pid;                 /* Parent process ID */

    /* === Sleep Support (Phase 5) === */
    uint64_t            wake_tick;                  /* Tick to wake at (0 = not sleeping) */

    /* === User Mode Support (Phase 5) === */
    phys_addr_t         pml4_phys;                  /* Process page table physical address */
    void*               user_stack;                 /* User stack base (virtual) */
    uint64_t            user_stack_top;             /* User stack top (initial RSP) */
    uint64_t            user_rsp;                   /* Saved user RSP during syscall */
    void*               user_code;                  /* User code base (virtual) */
    size_t              user_code_size;             /* User code size */
} process_t;

/* =============================================================================
 * Process Management API
 * =============================================================================
 */

/**
 * Initialize the process management subsystem.
 * Creates the idle process (PID 0) which runs when no other process is ready.
 */
void process_init(void);

/**
 * Create a new kernel process.
 *
 * The process will be added to the scheduler's run queue and will start
 * executing when scheduled. The entry function receives 'arg' as its parameter.
 *
 * @param name  Process name (for debugging, max PROCESS_NAME_MAX-1 chars)
 * @param entry Entry point function (void entry(void* arg))
 * @param arg   Argument to pass to entry point (can be NULL)
 * @return      PID of new process on success, (pid_t)-1 on failure
 */
pid_t process_create(const char* name, void (*entry)(void*), void* arg);

/**
 * Terminate the current process.
 *
 * This function does not return. The process is marked as TERMINATED
 * and the scheduler picks the next process to run.
 *
 * @param exit_code Exit code to store (retrievable by parent in future)
 */
NORETURN void process_exit(int exit_code);

/**
 * Voluntarily yield the CPU to another process.
 *
 * The current process is moved to the end of the run queue and the
 * scheduler picks the next process to run.
 */
void process_yield(void);

/**
 * Get the currently running process.
 *
 * @return Pointer to current process PCB, never NULL after process_init()
 */
process_t* process_current(void);

/**
 * Get a process by PID.
 *
 * @param pid Process ID to look up
 * @return    Pointer to PCB, or NULL if not found or terminated
 */
process_t* process_get(pid_t pid);

/**
 * Block the current process.
 *
 * The process is moved to BLOCKED state and removed from the run queue.
 * It will not be scheduled until unblocked. Used by I/O operations,
 * sleep, and synchronization primitives.
 */
void process_block(void);

/**
 * Unblock a process.
 *
 * The process is moved from BLOCKED to READY state and added to the
 * run queue. If the process is not in BLOCKED state, this is a no-op.
 *
 * @param pid Process ID to unblock
 */
void process_unblock(pid_t pid);

/**
 * Wake up sleeping processes whose wake_tick has passed.
 *
 * Called from the timer tick handler to unblock processes
 * that have finished sleeping.
 *
 * @param current_tick Current system tick count
 */
void process_wake_sleeping(uint64_t current_tick);

/**
 * Get count of processes in a given state.
 *
 * @param state State to count, or -1 to count all non-UNUSED processes
 * @return      Number of processes in that state
 */
uint32_t process_count(int state);

/**
 * Process entry point wrapper.
 * Called by context switch to start a new process.
 * Enables interrupts, calls the entry function, and exits on return.
 */
void process_entry_wrapper(void);

/* =============================================================================
 * Process State Names (for debugging)
 * =============================================================================
 */

extern const char* process_state_names[];

#endif /* CHANUX_PROCESS_H */
