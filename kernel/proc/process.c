/**
 * =============================================================================
 * Chanux OS - Process Management Implementation
 * =============================================================================
 * Implements process creation, termination, and lifecycle management.
 *
 * Process Table:
 *   - Fixed array of MAX_PROCESSES PCBs
 *   - PID 0 reserved for idle process
 *   - Linear scan for free slots (simple but O(n))
 *
 * Stack Setup:
 *   - Each process gets KERNEL_STACK_SIZE bytes
 *   - Stack grows downward (high to low addresses)
 *   - Initial stack frame set up for context_switch to "return" to
 * =============================================================================
 */

#include "../include/proc/process.h"
#include "../include/proc/sched.h"
#include "../include/mm/heap.h"
#include "../include/kernel.h"
#include "../drivers/vga/vga.h"
#include "../include/gdt.h"

/* =============================================================================
 * Static Data
 * =============================================================================
 */

/* Process table - all PCBs */
static process_t process_table[MAX_PROCESSES];

/* Next PID to assign */
static pid_t next_pid = 0;

/* Currently running process */
static process_t* current_process = NULL;

/* Process state names for debugging */
const char* process_state_names[] = {
    "UNUSED",
    "READY",
    "RUNNING",
    "BLOCKED",
    "TERMINATED"
};

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * Find a free slot in the process table.
 * Returns NULL if no free slots available.
 */
static process_t* find_free_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_STATE_UNUSED) {
            return &process_table[i];
        }
    }
    return NULL;
}

/**
 * Copy string with length limit.
 */
static void str_copy(char* dest, const char* src, size_t max) {
    size_t i;
    for (i = 0; i < max - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/**
 * Set up the initial kernel stack for a new process.
 *
 * The stack is set up so that when context_switch() restores the
 * callee-saved registers and returns, it "returns" to process_entry_wrapper.
 *
 * Stack layout (top to bottom, high to low address):
 *   - Return address (process_entry_wrapper)
 *   - rbx (0)
 *   - rbp (0)
 *   - r12 (0)
 *   - r13 (0)
 *   - r14 (0)
 *   - r15 (0)
 *   <- rsp points here after setup
 */
static void setup_initial_stack(process_t* proc) {
    uint64_t* stack = (uint64_t*)proc->kernel_stack_top;

    /* Push return address - where context_switch will "return" to */
    *(--stack) = (uint64_t)process_entry_wrapper;

    /* Push callee-saved registers (context_switch expects these) */
    *(--stack) = 0;  /* rbx */
    *(--stack) = 0;  /* rbp */
    *(--stack) = 0;  /* r12 */
    *(--stack) = 0;  /* r13 */
    *(--stack) = 0;  /* r14 */
    *(--stack) = 0;  /* r15 */

    /* Save the stack pointer */
    proc->rsp = (uint64_t)stack;
}

/* =============================================================================
 * Idle Process
 * =============================================================================
 */

/**
 * Idle process entry point.
 * Runs when no other process is ready.
 * Just halts the CPU until the next interrupt.
 */
static void idle_process_entry(void* arg) {
    (void)arg;

    for (;;) {
        /* Enable interrupts and halt until next interrupt */
        halt();
    }
}

/* =============================================================================
 * Process Management API Implementation
 * =============================================================================
 */

/**
 * Initialize the process management subsystem.
 */
void process_init(void) {
    /* Clear process table */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = PROCESS_STATE_UNUSED;
        process_table[i].pid = 0;
        process_table[i].kernel_stack = NULL;
    }

    /* Reset PID counter */
    next_pid = 0;

    /* Create idle process (PID 0) */
    process_t* idle = &process_table[0];

    idle->pid = next_pid++;
    str_copy(idle->name, "idle", PROCESS_NAME_MAX);
    idle->state = PROCESS_STATE_READY;
    idle->flags = PROCESS_FLAG_KERNEL | PROCESS_FLAG_IDLE;
    idle->priority = 0;  /* Lowest priority */
    idle->time_slice = DEFAULT_TIME_SLICE;
    idle->total_ticks = 0;
    idle->exit_code = 0;
    idle->parent_pid = 0;
    idle->next = NULL;
    idle->prev = NULL;

    /* Allocate kernel stack for idle process */
    idle->kernel_stack = kmalloc(KERNEL_STACK_SIZE);
    if (!idle->kernel_stack) {
        PANIC("Failed to allocate idle process stack");
    }

    /* Calculate stack top (stack grows downward, must be 16-byte aligned) */
    idle->kernel_stack_top = ((uint64_t)idle->kernel_stack + KERNEL_STACK_SIZE) & ~0xFULL;

    /* Set up entry point */
    idle->entry = idle_process_entry;
    idle->entry_arg = NULL;

    /* Set up initial stack frame */
    setup_initial_stack(idle);

    /* Idle process is special - it's the initial current process */
    current_process = idle;

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[PROC] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Idle process created (PID 0)\n");
}

/**
 * Create a new kernel process.
 */
pid_t process_create(const char* name, void (*entry)(void*), void* arg) {
    /* Disable interrupts during process creation */
    cli();

    /* Find a free PCB slot */
    process_t* proc = find_free_slot();
    if (!proc) {
        sti();
        kprintf("[PROC] Error: No free process slots\n");
        return (pid_t)-1;
    }

    /* Clean up any old kernel stack from previous use of this slot */
    if (proc->kernel_stack) {
        kfree(proc->kernel_stack);
        proc->kernel_stack = NULL;
    }

    /* Allocate kernel stack */
    void* stack = kmalloc(KERNEL_STACK_SIZE);
    if (!stack) {
        sti();
        kprintf("[PROC] Error: Failed to allocate stack for '%s'\n", name);
        return (pid_t)-1;
    }

    /* Initialize PCB */
    proc->pid = next_pid++;
    str_copy(proc->name, name ? name : "unnamed", PROCESS_NAME_MAX);
    proc->state = PROCESS_STATE_READY;
    proc->flags = PROCESS_FLAG_KERNEL;
    proc->priority = 1;  /* Default priority */
    proc->time_slice = DEFAULT_TIME_SLICE;
    proc->total_ticks = 0;
    proc->exit_code = 0;
    proc->parent_pid = current_process ? current_process->pid : 0;
    proc->next = NULL;
    proc->prev = NULL;

    /* Set up stack */
    proc->kernel_stack = stack;
    proc->kernel_stack_top = ((uint64_t)stack + KERNEL_STACK_SIZE) & ~0xFULL;

    /* Set up entry point */
    proc->entry = entry;
    proc->entry_arg = arg;

    /* Set up initial stack frame */
    setup_initial_stack(proc);

    /* Add to scheduler's run queue */
    sched_add(proc);

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[PROC] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Created process '%s' (PID %d)\n", proc->name, (int)proc->pid);

    sti();
    return proc->pid;
}

/**
 * Terminate the current process.
 */
NORETURN void process_exit(int exit_code) {
    cli();

    if (!current_process) {
        PANIC("process_exit called with no current process");
    }

    /* Can't exit the idle process */
    if (current_process->flags & PROCESS_FLAG_IDLE) {
        PANIC("Attempted to exit idle process");
    }

    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("[PROC] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Process '%s' (PID %d) exiting with code %d\n",
            current_process->name, (int)current_process->pid, exit_code);

    /* Mark as terminated */
    current_process->state = PROCESS_STATE_TERMINATED;
    current_process->exit_code = exit_code;

    /*
     * IMPORTANT: Do NOT free kernel_stack here!
     * We are still running on this stack. The stack will be freed
     * when this process slot is reused in process_create().
     */

    /* Switch to another process (never returns for this process) */
    schedule();

    /* Should never reach here */
    PANIC("process_exit: schedule returned");
    for (;;) halt();
}

/**
 * Voluntarily yield the CPU.
 */
void process_yield(void) {
    cli();

    if (!current_process) {
        sti();
        return;
    }

    /* Reset time slice and reschedule */
    current_process->time_slice = DEFAULT_TIME_SLICE;
    sched_reschedule();
    schedule();

    sti();
}

/**
 * Get the currently running process.
 */
process_t* process_current(void) {
    return current_process;
}

/**
 * Set the current process (called by scheduler).
 */
void process_set_current(process_t* proc) {
    current_process = proc;
}

/**
 * Get a process by PID.
 */
process_t* process_get(pid_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROCESS_STATE_UNUSED &&
            process_table[i].pid == pid) {
            return &process_table[i];
        }
    }
    return NULL;
}

/**
 * Block the current process.
 */
void process_block(void) {
    cli();

    if (!current_process) {
        sti();
        return;
    }

    /* Can't block idle process */
    if (current_process->flags & PROCESS_FLAG_IDLE) {
        sti();
        return;
    }

    current_process->state = PROCESS_STATE_BLOCKED;
    schedule();

    sti();
}

/**
 * Unblock a process.
 */
void process_unblock(pid_t pid) {
    cli();

    process_t* proc = process_get(pid);
    if (proc && proc->state == PROCESS_STATE_BLOCKED) {
        proc->state = PROCESS_STATE_READY;
        proc->time_slice = DEFAULT_TIME_SLICE;
        sched_add(proc);
    }

    sti();
}

/**
 * Get process count by state.
 */
uint32_t process_count(int state) {
    uint32_t count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (state < 0) {
            /* Count all non-unused processes */
            if (process_table[i].state != PROCESS_STATE_UNUSED) {
                count++;
            }
        } else {
            /* Count processes in specific state */
            if (process_table[i].state == (process_state_t)state) {
                count++;
            }
        }
    }
    return count;
}

/**
 * Wake sleeping processes whose wake_tick has passed.
 */
void process_wake_sleeping(uint64_t current_tick) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = &process_table[i];
        /* Check if process is blocked and has a wake_tick set */
        if (proc->state == PROCESS_STATE_BLOCKED && proc->wake_tick > 0) {
            /* Check if it's time to wake up */
            if (current_tick >= proc->wake_tick) {
                /* Unblock the process */
                proc->state = PROCESS_STATE_READY;
                proc->wake_tick = 0;
                proc->time_slice = DEFAULT_TIME_SLICE;
                sched_add(proc);
            }
        }
    }
}

/**
 * Process entry point wrapper.
 *
 * This is where new processes start executing. Context switch "returns" here.
 * We enable interrupts, call the actual entry point, and exit on return.
 */
void process_entry_wrapper(void) {
    /* Enable interrupts for this process */
    sti();

    /* Get current process */
    process_t* proc = process_current();

    /* Call the actual entry point */
    if (proc && proc->entry) {
        proc->entry(proc->entry_arg);
    }

    /* If the process returns, exit with code 0 */
    process_exit(0);
}
