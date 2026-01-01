/**
 * =============================================================================
 * Chanux OS - Process Control System Calls
 * =============================================================================
 * Implements system calls for process management:
 *   - sys_exit: Terminate the current process
 *   - sys_yield: Voluntarily yield CPU to other processes
 *   - sys_getpid: Get the current process ID
 *   - sys_sleep: Sleep for a specified number of milliseconds
 * =============================================================================
 */

#include "syscall/syscall.h"
#include "proc/process.h"
#include "proc/sched.h"
#include "kernel.h"
#include "drivers/pit.h"
#include "drivers/vga/vga.h"

/* =============================================================================
 * sys_exit - Terminate Current Process
 * =============================================================================
 * Terminates the calling process with the specified exit code.
 * This function does not return.
 *
 * @param code Exit code (0 = success, non-zero = error)
 */
NORETURN void sys_exit(int code) {
    process_t* current = process_current();

    kprintf("syscall: Process '%s' (PID %d) exiting with code %d\n",
            current->name, current->pid, code);

    /* Use the existing process_exit function */
    process_exit(code);

    /* Never reached */
    __builtin_unreachable();
}

/* =============================================================================
 * sys_yield - Voluntarily Yield CPU
 * =============================================================================
 * The calling process voluntarily gives up the CPU, allowing other
 * ready processes to run. The process remains in the READY state
 * and will be scheduled again later.
 *
 * @return 0 on success
 */
int64_t sys_yield(void) {
    process_yield();
    return 0;
}

/* =============================================================================
 * sys_getpid - Get Process ID
 * =============================================================================
 * Returns the process ID of the calling process.
 *
 * @return Process ID (always >= 0)
 */
int64_t sys_getpid(void) {
    process_t* current = process_current();
    return (int64_t)current->pid;
}

/* =============================================================================
 * sys_sleep - Sleep for Milliseconds
 * =============================================================================
 * Suspends the calling process for the specified number of milliseconds.
 * The process is moved to BLOCKED state and will be unblocked when
 * the sleep time expires.
 *
 * @param ms Number of milliseconds to sleep
 * @return   0 on success, negative on error
 */
int64_t sys_sleep(uint64_t ms) {
    if (ms == 0) {
        /* Sleep(0) is equivalent to yield */
        process_yield();
        return 0;
    }

    process_t* current = process_current();

    /* Calculate wake tick */
    /* PIT runs at 100Hz, so 1 tick = 10ms */
    uint64_t current_tick = pit_get_ticks();
    uint64_t ticks_to_sleep = (ms + 9) / 10;  /* Round up */
    current->wake_tick = current_tick + ticks_to_sleep;

    /* Block the process */
    process_block();

    /* When we return here, we've been unblocked */
    current->wake_tick = 0;

    return 0;
}
