/**
 * =============================================================================
 * Chanux OS - User Space Syscall Header
 * =============================================================================
 * Provides syscall wrappers for user-mode programs.
 *
 * This file defines the system call numbers and C wrapper functions
 * that user programs use to request kernel services.
 * =============================================================================
 */

#ifndef CHANUX_USER_SYSCALL_H
#define CHANUX_USER_SYSCALL_H

/* =============================================================================
 * Basic Types (standalone for user space)
 * =============================================================================
 */

typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef char int8_t;

typedef uint64_t size_t;
typedef int64_t ssize_t;
typedef int32_t pid_t;

#define NULL ((void*)0)

/* =============================================================================
 * System Call Numbers
 * =============================================================================
 * These must match the kernel's syscall table in syscall/syscall.c
 */

#define SYS_EXIT        0       /* void exit(int code) */
#define SYS_WRITE       1       /* ssize_t write(int fd, const void* buf, size_t len) */
#define SYS_READ        2       /* ssize_t read(int fd, void* buf, size_t len) */
#define SYS_YIELD       3       /* int yield(void) */
#define SYS_GETPID      4       /* pid_t getpid(void) */
#define SYS_SLEEP       5       /* int sleep(uint64_t ms) */

/* =============================================================================
 * Raw Syscall Interface
 * =============================================================================
 * Low-level function that invokes the SYSCALL instruction.
 * Implemented in assembly (syscall.asm).
 */

/**
 * Invoke a system call with up to 5 arguments.
 *
 * @param num   System call number
 * @param arg1  First argument
 * @param arg2  Second argument
 * @param arg3  Third argument
 * @param arg4  Fourth argument
 * @param arg5  Fifth argument
 * @return      System call return value (in RAX)
 */
extern int64_t syscall_raw(uint64_t num, uint64_t arg1, uint64_t arg2,
                           uint64_t arg3, uint64_t arg4, uint64_t arg5);

/* Convenience macros for different argument counts */
#define syscall0(num) \
    syscall_raw(num, 0, 0, 0, 0, 0)

#define syscall1(num, a1) \
    syscall_raw(num, (uint64_t)(a1), 0, 0, 0, 0)

#define syscall2(num, a1, a2) \
    syscall_raw(num, (uint64_t)(a1), (uint64_t)(a2), 0, 0, 0)

#define syscall3(num, a1, a2, a3) \
    syscall_raw(num, (uint64_t)(a1), (uint64_t)(a2), (uint64_t)(a3), 0, 0)

#define syscall4(num, a1, a2, a3, a4) \
    syscall_raw(num, (uint64_t)(a1), (uint64_t)(a2), (uint64_t)(a3), (uint64_t)(a4), 0)

#define syscall5(num, a1, a2, a3, a4, a5) \
    syscall_raw(num, (uint64_t)(a1), (uint64_t)(a2), (uint64_t)(a3), (uint64_t)(a4), (uint64_t)(a5))

/* =============================================================================
 * High-Level Syscall Wrappers
 * =============================================================================
 * These are the C functions that user programs should call.
 */

/**
 * Terminate the current process.
 *
 * @param code Exit code (returned to parent)
 */
void exit(int code) __attribute__((noreturn));

/**
 * Write data to a file descriptor.
 *
 * @param fd  File descriptor (1 = stdout, 2 = stderr)
 * @param buf Buffer containing data to write
 * @param len Number of bytes to write
 * @return    Number of bytes written, or negative error code
 */
ssize_t write(int fd, const void* buf, size_t len);

/**
 * Read data from a file descriptor.
 *
 * @param fd  File descriptor (0 = stdin)
 * @param buf Buffer to store read data
 * @param len Maximum bytes to read
 * @return    Number of bytes read, or negative error code
 */
ssize_t read(int fd, void* buf, size_t len);

/**
 * Yield CPU to another process.
 *
 * @return 0 on success
 */
int yield(void);

/**
 * Get current process ID.
 *
 * @return Process ID
 */
pid_t getpid(void);

/**
 * Sleep for specified milliseconds.
 *
 * @param ms Milliseconds to sleep
 * @return   0 on success, negative on error
 */
int sleep(uint64_t ms);

/* =============================================================================
 * Convenience Functions
 * =============================================================================
 */

/**
 * Print a string to stdout.
 *
 * @param s Null-terminated string
 * @return  Number of bytes written
 */
ssize_t puts(const char* s);

/**
 * Calculate string length.
 *
 * @param s Null-terminated string
 * @return  Length (not including null terminator)
 */
size_t strlen(const char* s);

/* =============================================================================
 * Number Printing Functions
 * =============================================================================
 */

/**
 * Print an unsigned integer to stdout.
 */
void print_uint(uint64_t value);

/**
 * Print a signed integer to stdout.
 */
void print_int(int64_t value);

/**
 * Print an unsigned integer in hexadecimal to stdout.
 */
void print_hex(uint64_t value);

/* =============================================================================
 * Memory Functions
 * =============================================================================
 */

/**
 * Set memory to a value.
 */
void* memset(void* dest, int val, size_t count);

/**
 * Copy memory.
 */
void* memcpy(void* dest, const void* src, size_t count);

#endif /* CHANUX_USER_SYSCALL_H */
