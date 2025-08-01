/*
 * Process Scheduler Implementation
 * 
 * Implements a simple round-robin scheduler for process management.
 */

#include "kernel/scheduler.h"
#include "arch/x86/tss.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/heap.h"
#include "kernel/timer.h"
#include "lib/string.h"
#include <stddef.h>

/* External functions */
void terminal_writestring(const char* data);
void terminal_write_dec(uint32_t value);

/* Scheduler state */
static process_t* process_list[MAX_PROCESSES];  /* Array of all processes */
static process_t* current_process = NULL;       /* Currently running process */
static process_t* ready_queue_head = NULL;      /* Head of ready queue */
static process_t* ready_queue_tail = NULL;      /* Tail of ready queue */
static pid_t next_pid = 1;                      /* Next available PID */
static scheduler_stats_t stats = {0};           /* Scheduler statistics */

/* Time slice in timer ticks (10ms per tick, 50ms time slice) */
#define TIME_SLICE_TICKS 5

/* Forward declarations */
static void add_to_ready_queue(process_t* proc);
static process_t* remove_from_ready_queue(void);
static void schedule(void);

/*
 * scheduler_init - Initialize the scheduler
 * 
 * Creates the idle process and sets up scheduler data structures.
 */
void scheduler_init(void) {
    terminal_writestring("Initializing scheduler...\n");
    
    /* Clear process list */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_list[i] = NULL;
    }
    
    /* Initialize TSS for privilege level switching */
    tss_init();
    
    /* Create idle process (PID 0) */
    process_t* idle = (process_t*)malloc(sizeof(process_t));
    if (!idle) {
        terminal_writestring("Failed to allocate idle process!\n");
        return;
    }
    
    memset(idle, 0, sizeof(process_t));
    idle->pid = 0;
    strcpy(idle->name, "idle");
    idle->state = PROCESS_STATE_RUNNING;
    idle->time_slice = TIME_SLICE_TICKS;
    idle->page_directory = vmm_get_current_directory();
    
    /* Set idle process as current */
    process_list[0] = idle;
    current_process = idle;
    
    terminal_writestring("Scheduler initialized\n");
}

/*
 * process_create - Create a new process
 * @name: Process name
 * @entry_point: Function to execute
 * 
 * Returns: PID of created process, or -1 on error
 */
pid_t process_create(const char* name, void (*entry_point)(void)) {
    /* Find free slot in process list */
    int slot = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (process_list[i] == NULL) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        terminal_writestring("No free process slots!\n");
        return -1;
    }
    
    /* Allocate PCB */
    process_t* proc = (process_t*)malloc(sizeof(process_t));
    if (!proc) {
        terminal_writestring("Failed to allocate PCB!\n");
        return -1;
    }
    
    /* Initialize PCB */
    memset(proc, 0, sizeof(process_t));
    proc->pid = next_pid++;
    strncpy(proc->name, name, 31);
    proc->name[31] = '\0';
    proc->state = PROCESS_STATE_READY;
    proc->time_slice = TIME_SLICE_TICKS;
    proc->parent_pid = current_process ? current_process->pid : 0;
    proc->start_time = timer_get_ticks();
    
    /* Allocate kernel stack */
    uint32_t kernel_stack = pmm_alloc_page();
    if (!kernel_stack) {
        free(proc);
        terminal_writestring("Failed to allocate kernel stack!\n");
        return -1;
    }
    proc->kernel_stack = kernel_stack + PROCESS_STACK_SIZE;
    
    /* Create new page directory (clone current) */
    proc->page_directory = vmm_create_page_directory();
    if (!proc->page_directory) {
        pmm_free_page(kernel_stack);
        free(proc);
        terminal_writestring("Failed to create page directory!\n");
        return -1;
    }
    
    /* Set up initial stack for context switch */
    uint32_t* stack_ptr = (uint32_t*)(proc->kernel_stack - sizeof(cpu_context_t));
    
    /* Push initial context */
    stack_ptr[0] = 0;                    /* EDI */
    stack_ptr[1] = 0;                    /* ESI */
    stack_ptr[2] = 0;                    /* EBX */
    stack_ptr[3] = 0;                    /* EBP */
    stack_ptr[4] = (uint32_t)entry_point; /* Return address for process_start */
    
    proc->context.esp = (uint32_t)stack_ptr;
    
    /* Add to process list and ready queue */
    process_list[slot] = proc;
    add_to_ready_queue(proc);
    
    stats.processes_created++;
    
    terminal_writestring("Created process '");
    terminal_writestring(name);
    terminal_writestring("' with PID ");
    terminal_write_dec(proc->pid);
    terminal_writestring("\n");
    
    return proc->pid;
}

/*
 * process_exit - Terminate current process
 * @status: Exit status (unused for now)
 */
void process_exit(int status) {
    (void)status; /* Suppress unused parameter warning */
    if (current_process->pid == 0) {
        terminal_writestring("Cannot exit idle process!\n");
        return;
    }
    
    terminal_writestring("Process ");
    terminal_write_dec(current_process->pid);
    terminal_writestring(" exiting\n");
    
    /* Mark process as terminated */
    current_process->state = PROCESS_STATE_TERMINATED;
    
    /* Free resources (simplified - full cleanup would include memory, files, etc.) */
    pmm_free_page(current_process->kernel_stack - PROCESS_STACK_SIZE);
    
    /* Find and clear process slot */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i] == current_process) {
            process_list[i] = NULL;
            break;
        }
    }
    
    stats.processes_terminated++;
    
    /* Schedule next process */
    schedule();
}

/*
 * process_yield - Yield CPU to next process
 */
void process_yield(void) {
    current_process->state = PROCESS_STATE_READY;
    add_to_ready_queue(current_process);
    schedule();
}

/*
 * process_get_current - Get current process
 */
process_t* process_get_current(void) {
    return current_process;
}

/*
 * process_get_current_pid - Get current process PID
 */
pid_t process_get_current_pid(void) {
    return current_process ? current_process->pid : 0;
}

/*
 * scheduler_tick - Called on timer interrupt
 * 
 * Decrements time slice and schedules if needed.
 */
void scheduler_tick(void) {
    if (!current_process) return;

    /* Update CPU time */
    current_process->cpu_time++;
    
    /* Check for sleeping processes that need to wake up */
    extern uint64_t timer_get_ticks(void);
    uint64_t current_ticks = timer_get_ticks();
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = process_list[i];
        if (proc && proc->state == PROCESS_STATE_BLOCKED && 
            proc->wake_time > 0 && current_ticks >= proc->wake_time) {
            /* Wake up the process */
            proc->wake_time = 0;
            proc->state = PROCESS_STATE_READY;
            add_to_ready_queue(proc);
        }
    }

    /* Check time slice */
    if (current_process->time_slice > 0) {
        current_process->time_slice--;
    }
    
    /* Time slice expired - schedule next process */
    if (current_process->time_slice == 0) {
        /* Only add to ready queue if not idle process */
        if (current_process->pid != 0) {
            current_process->state = PROCESS_STATE_READY;
            add_to_ready_queue(current_process);
        }
        schedule();
    }
}

/*
 * add_to_ready_queue - Add process to ready queue
 */
static void add_to_ready_queue(process_t* proc) {
    if (!proc || proc->state != PROCESS_STATE_READY) return;
    
    proc->next = NULL;
    
    if (!ready_queue_head) {
        ready_queue_head = ready_queue_tail = proc;
    } else {
        ready_queue_tail->next = proc;
        ready_queue_tail = proc;
    }
}

/*
 * remove_from_ready_queue - Remove and return first process from ready queue
 */
static process_t* remove_from_ready_queue(void) {
    if (!ready_queue_head) return NULL;
    
    process_t* proc = ready_queue_head;
    ready_queue_head = ready_queue_head->next;
    
    if (!ready_queue_head) {
        ready_queue_tail = NULL;
    }
    
    proc->next = NULL;
    return proc;
}

/*
 * schedule - Select and switch to next process
 */
static void schedule(void) {
    /* Save current process context is handled by switch_context */
    
    /* Get next process from ready queue */
    process_t* next = remove_from_ready_queue();
    
    /* If no ready process, run idle */
    if (!next) {
        next = process_list[0];  /* Idle process */
    }
    
    /* If same process, just reset time slice */
    if (next == current_process) {
        current_process->time_slice = TIME_SLICE_TICKS;
        return;
    }
    
    /* Perform context switch */
    process_t* prev = current_process;
    current_process = next;
    current_process->state = PROCESS_STATE_RUNNING;
    current_process->time_slice = TIME_SLICE_TICKS;
    
    /* Update TSS with new kernel stack */
    tss_set_kernel_stack(current_process->kernel_stack);
    
    /* Switch page directory if different */
    if (prev->page_directory != current_process->page_directory) {
        vmm_switch_page_directory(current_process->page_directory);
    }
    
    stats.context_switches++;
    
    /* Perform the actual context switch */
    switch_context(&prev->context, &current_process->context);
}


/*
 * process_sleep - Put current process to sleep for specified milliseconds
 * @ms: Number of milliseconds to sleep
 */
void process_sleep(uint32_t ms) {
    if (!current_process || current_process->pid == 0) {
        return;  /* Can't sleep idle process */
    }
    
    /* Calculate wake time */
    extern uint64_t timer_get_ticks(void);
    extern uint32_t timer_get_frequency(void);
    uint32_t ticks_per_sec = timer_get_frequency();
    uint32_t ms_per_tick = 1000 / ticks_per_sec;
    uint64_t ticks_to_sleep = (ms + ms_per_tick - 1) / ms_per_tick;
    
    current_process->wake_time = timer_get_ticks() + ticks_to_sleep;
    current_process->state = PROCESS_STATE_BLOCKED;
    
    /* Schedule next process */
    schedule();
}

/*
 * scheduler_get_stats - Get scheduler statistics
 */
scheduler_stats_t scheduler_get_stats(void) {
    return stats;
}

/*
 * process_terminate - Terminate a process with given signal
 * @proc: Process to terminate
 * @signal: Signal number (for future use)
 */
void process_terminate(process_t* proc, int signal) {
    (void)signal; /* Suppress unused parameter warning */
    
    if (!proc || proc->pid == 0) {
        terminal_writestring("Cannot terminate idle process!\n");
        return;
    }
    
    terminal_writestring("Terminating process: ");
    terminal_writestring(proc->name);
    terminal_writestring(" (PID: ");
    terminal_write_dec(proc->pid);
    terminal_writestring(")\n");
    
    /* Mark process as terminated */
    proc->state = PROCESS_STATE_TERMINATED;
    
    /* If terminating current process, schedule next */
    if (proc == current_process) {
        schedule();
        /* Should not return here */
    }
    
    stats.processes_terminated++;
}