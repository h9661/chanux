/**
 * =============================================================================
 * Chanux OS - Init (First User Program)
 * =============================================================================
 * This is the first user-mode program that runs on Chanux.
 * It demonstrates:
 *   - Running in Ring 3 (user mode)
 *   - Making system calls
 *   - Basic I/O operations
 * =============================================================================
 */

#include "../include/syscall.h"

/**
 * Entry point for the init program.
 * This is called by the startup code in start.asm.
 */
void main(void) {
    /* Print welcome message */
    puts("=================================\n");
    puts("  Welcome to Chanux User Space!\n");
    puts("=================================\n\n");

    /* Get and print our PID */
    pid_t pid = getpid();
    puts("Init process running (PID: ");
    print_uint((uint64_t)pid);
    puts(")\n\n");

    /* Demonstrate system calls */
    puts("Testing system calls:\n");

    /* Test write */
    puts("  [OK] write() works\n");

    /* Test yield */
    yield();
    puts("  [OK] yield() returned\n");

    /* Test sleep */
    puts("  Sleeping for 500ms...\n");
    sleep(500);
    puts("  [OK] sleep() returned\n");

    /* Print final message */
    puts("\n");
    puts("All tests passed!\n");
    puts("Init process exiting normally.\n");

    /* Exit cleanly */
    exit(0);
}
