/**
 * =============================================================================
 * Chanux OS - Process Scheduler
 * =============================================================================
 * Implements a preemptive round-robin scheduler.
 *
 * Scheduling Algorithm:
 *   - Round-robin with fixed time slices (100ms default)
 *   - Timer-based preemption via PIT IRQ0
 *   - Single run queue (FIFO ordering)
 *   - Idle process runs when no other process is ready
 *
 * Integration:
 *   - PIT timer calls sched_tick() on each IRQ0
 *   - Context switches update TSS.RSP0 for future interrupt handling
 *   - Processes can voluntarily yield via process_yield()
 * =============================================================================
 */

#ifndef CHANUX_SCHED_H
#define CHANUX_SCHED_H

#include "process.h"

/* =============================================================================
 * Scheduler Configuration
 * =============================================================================
 */

#define SCHED_TICK_RATE         100     /* Scheduler runs at 100Hz (PIT rate) */
#define SCHED_MIN_TIME_SLICE    1       /* Minimum time slice (1 tick = 10ms) */
#define SCHED_MAX_TIME_SLICE    100     /* Maximum time slice (1 second) */

/* =============================================================================
 * Scheduler API
 * =============================================================================
 */

/**
 * Initialize the scheduler.
 *
 * Must be called after process_init(). Sets up the run queue and
 * prepares for multitasking. Does not start scheduling yet.
 */
void sched_init(void);

/**
 * Start the scheduler and begin multitasking.
 *
 * This function never returns. It performs the initial context switch
 * to the first ready process and enables timer-based preemption.
 *
 * Prerequisites:
 *   - process_init() has been called
 *   - sched_init() has been called
 *   - At least one process (besides idle) has been created
 */
NORETURN void sched_start(void);

/**
 * Timer tick handler.
 *
 * Called from PIT IRQ0 handler on each timer interrupt (100Hz).
 * Decrements the current process's time slice and triggers a
 * reschedule if the time quantum has expired.
 *
 * @param regs Pointer to saved registers (for context switch if needed)
 */
void sched_tick(registers_t* regs);

/**
 * Request a reschedule at the next opportunity.
 *
 * Sets a flag that causes the scheduler to run at the end of the
 * current interrupt handler or system call.
 */
void sched_reschedule(void);

/**
 * Perform a context switch to the next ready process.
 *
 * Called internally by sched_tick() or when a process blocks/yields.
 * Saves the current context and switches to the next process.
 */
void schedule(void);

/**
 * Add a process to the run queue.
 *
 * The process is added to the tail of the queue (FIFO ordering).
 * Its state is set to READY if not already.
 *
 * @param proc Process to add (must not be NULL)
 */
void sched_add(process_t* proc);

/**
 * Remove a process from the run queue.
 *
 * Used when a process blocks or terminates.
 *
 * @param proc Process to remove (must not be NULL)
 */
void sched_remove(process_t* proc);

/**
 * Pick the next process to run.
 *
 * Removes and returns the process at the head of the run queue.
 * If the queue is empty, returns the idle process.
 *
 * @return Pointer to next process to run (never NULL)
 */
process_t* sched_pick_next(void);

/**
 * Check if the scheduler is currently running.
 *
 * @return true if scheduler has been started, false otherwise
 */
bool sched_is_running(void);

/**
 * Get the idle process.
 *
 * @return Pointer to the idle process PCB
 */
process_t* sched_get_idle(void);

/**
 * Get the number of processes in the run queue.
 *
 * @return Number of ready processes (excluding idle)
 */
uint32_t sched_ready_count(void);

/* =============================================================================
 * Context Switch Functions (Assembly)
 * =============================================================================
 * These are implemented in context.asm
 */

/**
 * Perform a context switch between two processes.
 *
 * Saves callee-saved registers on the current stack, saves RSP to
 * *old_rsp_ptr, updates TSS.RSP0, optionally switches CR3, loads
 * the new RSP, and restores callee-saved registers from the new stack.
 *
 * @param old_rsp_ptr Pointer to save current RSP (in old process's PCB)
 * @param new_rsp     RSP value to load for new process
 * @param new_rsp0    RSP0 value for TSS (new process's kernel stack top)
 * @param new_cr3     CR3 value for new process (0 = don't switch, kernel process)
 */
extern void context_switch(uint64_t* old_rsp_ptr, uint64_t new_rsp,
                           uint64_t new_rsp0, uint64_t new_cr3);

/**
 * Perform the initial context switch to the first process.
 *
 * Similar to context_switch, but does not save the current context
 * (there is nothing to save initially).
 *
 * @param new_rsp  RSP value for first process
 * @param new_rsp0 RSP0 value for TSS
 * @param new_cr3  CR3 value for first process (0 = don't switch)
 */
extern void context_switch_first(uint64_t new_rsp, uint64_t new_rsp0, uint64_t new_cr3);

#endif /* CHANUX_SCHED_H */
