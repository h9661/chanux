/**
 * =============================================================================
 * Chanux OS - User Mode Support Header
 * =============================================================================
 * Declarations for user-mode process creation and management.
 * =============================================================================
 */

#ifndef CHANUX_USER_H
#define CHANUX_USER_H

#include "../types.h"
#include "../proc/process.h"
#include "../mm/vmm.h"

/* =============================================================================
 * User Space Layout
 * =============================================================================
 *
 * 0x0000_0000_0000_0000 - 0x0000_0000_003F_FFFF  Reserved (null guard)
 * 0x0000_0000_0040_0000 - 0x0000_0000_007F_FFFF  Code segment (4MB)
 * 0x0000_0000_0080_0000 - 0x0000_0000_00FF_FFFF  Data segment (8MB)
 * 0x0000_0000_1000_0000 - 0x0000_7FFF_FFEF_FFFF  Heap (grows up)
 * 0x0000_7FFF_FFF0_0000 - 0x0000_7FFF_FFFD_FFFF  Stack (grows down)
 * 0x0000_7FFF_FFFE_0000 - 0x0000_7FFF_FFFF_FFFF  Reserved
 * =============================================================================
 */

/* User code is loaded at 4MB */
#define USER_CODE_BASE      0x0000000000400000ULL

/* =============================================================================
 * User Stack Management
 * =============================================================================
 */

/**
 * Allocate and map a user stack for a process.
 *
 * @param proc Process to allocate stack for (must have pml4_phys set)
 * @return true on success, false on failure
 */
bool user_stack_alloc(process_t* proc);

/**
 * Free a user stack.
 *
 * @param proc Process whose stack to free
 */
void user_stack_free(process_t* proc);

/* =============================================================================
 * User Code Loading
 * =============================================================================
 */

/**
 * Load user code into a process's address space.
 *
 * @param proc      Process to load code into
 * @param code      Pointer to code data (in kernel space)
 * @param code_size Size of code in bytes
 * @param entry     Virtual address to load code at
 * @return true on success, false on failure
 */
bool user_load_code(process_t* proc, const void* code, size_t code_size,
                    virt_addr_t entry);

/* =============================================================================
 * User Process Creation
 * =============================================================================
 */

/**
 * Create a user-mode process.
 *
 * @param name      Process name
 * @param code      Pointer to user code (binary)
 * @param code_size Size of user code
 * @return PID on success, (pid_t)-1 on failure
 */
pid_t user_process_create(const char* name, const void* code, size_t code_size);

/* =============================================================================
 * User Mode Entry
 * =============================================================================
 */

/**
 * Enter user mode for the first time.
 * Sets up the stack frame for IRETQ and jumps to user code.
 * This function does not return.
 *
 * @param entry_point User mode entry point (RIP)
 * @param user_stack  User mode stack pointer (RSP)
 */
NORETURN void user_mode_enter(uint64_t entry_point, uint64_t user_stack);

/**
 * Set up a process to enter user mode.
 * Called during context switch to prepare for first user mode entry.
 *
 * @param proc Process to set up
 */
void user_mode_setup(process_t* proc);

#endif /* CHANUX_USER_H */
