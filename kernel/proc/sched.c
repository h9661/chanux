/**
 * =============================================================================
 * Chanux OS - Process Scheduler Implementation
 * =============================================================================
 * Implements a preemptive round-robin scheduler.
 *
 * Algorithm:
 *   - Each process gets a fixed time slice (DEFAULT_TIME_SLICE ticks)
 *   - When time slice expires, process goes to end of run queue
 *   - Next process is taken from head of run queue
 *   - If run queue is empty, idle process runs
 *
 * Run Queue:
 *   - Doubly-linked list for O(1) add/remove
 *   - Add to tail, remove from head (FIFO)
 *   - Idle process is never in the queue
 *
 * Preemption:
 *   - PIT timer (100Hz) calls sched_tick() on each interrupt
 *   - sched_tick() decrements time slice and reschedules if expired
 * =============================================================================
 */

#include "../include/proc/sched.h"
#include "../include/proc/process.h"
#include "../include/kernel.h"
#include "../include/gdt.h"
#include "../include/drivers/pit.h"
#include "../include/debug.h"
#include "../drivers/vga/vga.h"

/* =============================================================================
 * Static Data
 * =============================================================================
 */

/* Run queue head and tail */
static process_t* run_queue_head = NULL;
static process_t* run_queue_tail = NULL;

/* Scheduler state */
static bool scheduler_running = false;
static bool need_reschedule = false;

/* Pointer to idle process (PID 0) */
static process_t* idle_process = NULL;

/* =============================================================================
 * Run Queue Management
 * =============================================================================
 */

/**
 * Add a process to the run queue (at tail for FIFO ordering).
 */
void sched_add(process_t* proc) {
    if (!proc) return;

    /* Don't add idle process to queue */
    if (proc->flags & PROCESS_FLAG_IDLE) return;

    /* Don't add if already in queue */
    if (proc->next != NULL || proc->prev != NULL ||
        proc == run_queue_head) {
        return;
    }

    /* Set state to READY */
    if (proc->state != PROCESS_STATE_READY) {
        proc->state = PROCESS_STATE_READY;
    }

    /* Add to tail of queue */
    proc->prev = run_queue_tail;
    proc->next = NULL;

    if (run_queue_tail) {
        run_queue_tail->next = proc;
    } else {
        /* Queue was empty */
        run_queue_head = proc;
    }
    run_queue_tail = proc;
}

/**
 * Remove a process from the run queue.
 */
void sched_remove(process_t* proc) {
    if (!proc) return;

    /* Update previous node's next pointer */
    if (proc->prev) {
        proc->prev->next = proc->next;
    } else {
        /* Removing head */
        run_queue_head = proc->next;
    }

    /* Update next node's prev pointer */
    if (proc->next) {
        proc->next->prev = proc->prev;
    } else {
        /* Removing tail */
        run_queue_tail = proc->prev;
    }

    /* Clear the process's queue pointers */
    proc->next = NULL;
    proc->prev = NULL;
}

/**
 * Pick the next process to run.
 * Removes from head of queue. Returns idle if queue empty.
 */
process_t* sched_pick_next(void) {
    if (run_queue_head != NULL) {
        process_t* next = run_queue_head;

        /* Remove from head */
        run_queue_head = next->next;
        if (run_queue_head) {
            run_queue_head->prev = NULL;
        } else {
            run_queue_tail = NULL;
        }

        next->next = NULL;
        next->prev = NULL;

        return next;
    }

    /* Queue is empty - return idle process */
    return idle_process;
}

/**
 * Get number of processes in run queue.
 */
uint32_t sched_ready_count(void) {
    uint32_t count = 0;
    process_t* p = run_queue_head;
    while (p) {
        count++;
        p = p->next;
    }
    return count;
}

/* =============================================================================
 * Scheduler API Implementation
 * =============================================================================
 */

/**
 * Initialize the scheduler.
 */
void sched_init(void) {
    /* Get the idle process (created by process_init) */
    idle_process = process_get(0);
    if (!idle_process) {
        PANIC("sched_init: idle process not found");
    }

    /* Initialize run queue */
    run_queue_head = NULL;
    run_queue_tail = NULL;

    scheduler_running = false;
    need_reschedule = false;

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[SCHED] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Scheduler initialized (round-robin, %d ms quantum)\n",
            DEFAULT_TIME_SLICE * 10);
}

/**
 * Start the scheduler.
 * This function never returns.
 */
NORETURN void sched_start(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[SCHED] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Starting scheduler with %d ready processes\n", sched_ready_count());

    /* Pick the first process to run */
    process_t* first = sched_pick_next();
    if (!first) {
        PANIC("sched_start: no processes to run");
    }

    /* Set scheduler as running */
    scheduler_running = true;

    /* Update current process */
    first->state = PROCESS_STATE_RUNNING;
    first->time_slice = DEFAULT_TIME_SLICE;

    /* Update the current process pointer in process.c */
    extern void process_set_current(process_t* proc);
    process_set_current(first);

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("[SCHED] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Switching to first process: '%s' (PID %d)\n",
            first->name, (int)first->pid);

    /* Perform initial context switch (pass CR3 for user processes) */
    context_switch_first(first->rsp, first->kernel_stack_top, first->pml4_phys);

    /* Should never reach here */
    PANIC("sched_start: context_switch_first returned");
    for (;;) halt();
}

/**
 * Timer tick handler.
 * Called from PIT IRQ0 at 100Hz.
 */
void sched_tick(registers_t* regs) {
    (void)regs;  /* Unused in current implementation */

    if (!scheduler_running) return;

    /* Wake up any sleeping processes whose wake time has passed */
    process_wake_sleeping(pit_get_ticks());

    process_t* current = process_current();
    if (!current) return;

    /* Account CPU time */
    current->total_ticks++;

    /* Decrement time slice */
    if (current->time_slice > 0) {
        current->time_slice--;
    }

    /* Check for preemption */
    if (current->time_slice == 0) {
        /* Don't preempt idle if nothing else to run */
        if (!(current->flags & PROCESS_FLAG_IDLE) || run_queue_head != NULL) {
            need_reschedule = true;
        } else {
            /* Reset idle's time slice */
            current->time_slice = DEFAULT_TIME_SLICE;
        }
    }

    /* Perform context switch if needed */
    if (need_reschedule) {
        need_reschedule = false;
        schedule();
    }
}

/**
 * Request a reschedule.
 */
void sched_reschedule(void) {
    need_reschedule = true;
}

/**
 * Main scheduling function.
 * Saves current context and switches to next process.
 */
void schedule(void) {
    if (!scheduler_running) return;

    process_t* prev = process_current();
    if (!prev) return;

    process_t* next = sched_pick_next();
    if (!next) {
        next = idle_process;
    }

    DBG_SCHED("[SCHED] schedule: prev='%s' (PID %d, state=%d) -> next='%s' (PID %d)\n",
              prev->name, (int)prev->pid, prev->state,
              next->name, (int)next->pid);

    /* Same process - just reset time slice */
    if (next == prev) {
        next->time_slice = DEFAULT_TIME_SLICE;
        return;
    }

    /* Put previous process back in run queue if still runnable */
    if (prev->state == PROCESS_STATE_RUNNING) {
        prev->state = PROCESS_STATE_READY;
        prev->time_slice = DEFAULT_TIME_SLICE;
        sched_add(prev);
    }

    /* Switch to next process */
    next->state = PROCESS_STATE_RUNNING;
    next->time_slice = DEFAULT_TIME_SLICE;

    /* Update current process pointer */
    extern void process_set_current(process_t* proc);
    process_set_current(next);

    /* Perform context switch (pass CR3 for user processes, 0 for kernel) */
    context_switch(&prev->rsp, next->rsp, next->kernel_stack_top, next->pml4_phys);
}

/**
 * Check if scheduler is running.
 */
bool sched_is_running(void) {
    return scheduler_running;
}

/**
 * Get the idle process.
 */
process_t* sched_get_idle(void) {
    return idle_process;
}
