/**
 * =============================================================================
 * Chanux OS - I/O System Calls
 * =============================================================================
 * Implements system calls for input/output operations:
 *   - sys_write: Write data to a file descriptor
 *   - sys_read: Read data from a file descriptor
 *
 * Currently supported file descriptors:
 *   - 0 (stdin):  Keyboard input
 *   - 1 (stdout): VGA console output
 *   - 2 (stderr): VGA console output (same as stdout)
 * =============================================================================
 */

#include "syscall/syscall.h"
#include "kernel.h"
#include "drivers/vga/vga.h"
#include "drivers/keyboard.h"

/* =============================================================================
 * User Pointer Validation
 * =============================================================================
 */

/* User space address limit (canonical lower half) */
#define USER_SPACE_END  0x0000800000000000ULL

/**
 * Validate a user-space pointer.
 *
 * @param ptr Pointer to validate
 * @param len Length of memory region
 * @return    true if valid, false otherwise
 */
static bool validate_user_ptr(const void* ptr, size_t len) {
    uintptr_t addr = (uintptr_t)ptr;

    /* Null pointer check */
    if (ptr == NULL) {
        return false;
    }

    /* Must be in user space (lower canonical addresses) */
    if (addr >= USER_SPACE_END) {
        return false;
    }

    /* Check for overflow */
    if (addr + len < addr) {
        return false;
    }

    /* Check end address is also in user space */
    if (addr + len > USER_SPACE_END) {
        return false;
    }

    /* TODO: Check that pages are actually mapped with PTE_USER */
    /* For now, we trust the address is valid if it's in the range */

    return true;
}

/* =============================================================================
 * File Descriptor Constants
 * =============================================================================
 */

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* =============================================================================
 * sys_write - Write to File Descriptor
 * =============================================================================
 * Write data from a user buffer to a file descriptor.
 *
 * @param fd  File descriptor (0=stdin, 1=stdout, 2=stderr)
 * @param buf Pointer to data to write (user space)
 * @param len Number of bytes to write
 * @return    Number of bytes written on success, negative error on failure
 */
int64_t sys_write(int fd, const void* buf, size_t len) {
    /* Validate buffer pointer */
    if (!validate_user_ptr(buf, len)) {
        return -EFAULT;
    }

    /* Handle based on file descriptor */
    switch (fd) {
        case STDOUT_FILENO:
        case STDERR_FILENO: {
            /* Write to VGA console */
            const char* str = (const char*)buf;
            for (size_t i = 0; i < len; i++) {
                vga_putchar(str[i]);
            }
            return (int64_t)len;
        }

        case STDIN_FILENO:
            /* Cannot write to stdin */
            return -EBADF;

        default:
            /* Invalid file descriptor */
            return -EBADF;
    }
}

/* =============================================================================
 * sys_read - Read from File Descriptor
 * =============================================================================
 * Read data from a file descriptor into a user buffer.
 *
 * For stdin (fd=0), this reads from the keyboard buffer.
 * The call may block if no input is available.
 *
 * @param fd  File descriptor (0=stdin, 1=stdout, 2=stderr)
 * @param buf Pointer to buffer to fill (user space)
 * @param len Maximum number of bytes to read
 * @return    Number of bytes read on success, negative error on failure
 */
int64_t sys_read(int fd, void* buf, size_t len) {
    /* Validate buffer pointer */
    if (!validate_user_ptr(buf, len)) {
        return -EFAULT;
    }

    /* Handle based on file descriptor */
    switch (fd) {
        case STDIN_FILENO: {
            /* Read from keyboard */
            if (len == 0) {
                return 0;
            }

            char* str = (char*)buf;
            size_t count = 0;

            /* Try to read available characters (non-blocking) */
            while (count < len) {
                if (!keyboard_has_key()) {
                    /* No more input available */
                    break;
                }
                char c = keyboard_getchar_nonblock();
                if (c == 0) {
                    /* No more input available */
                    break;
                }
                str[count++] = c;
            }

            /* If no data available, we could block here */
            /* For now, return what we have (or 0 if nothing) */
            return (int64_t)count;
        }

        case STDOUT_FILENO:
        case STDERR_FILENO:
            /* Cannot read from stdout/stderr */
            return -EBADF;

        default:
            /* Invalid file descriptor */
            return -EBADF;
    }
}
