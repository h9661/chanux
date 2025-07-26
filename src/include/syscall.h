/*
 * System Call Interface
 * 
 * Defines system call numbers and interfaces for user-kernel communication
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

/* System call numbers */
#define SYS_EXIT        1
#define SYS_WRITE       2
#define SYS_READ        3
#define SYS_OPEN        4
#define SYS_CLOSE       5
#define SYS_GETPID      6
#define SYS_SLEEP       7

/* Maximum number of system calls */
#define MAX_SYSCALLS    8

/* System call interrupt number (Linux-compatible) */
#define SYSCALL_INT     0x80

/* Registers structure passed to system call handler */
struct syscall_regs {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   /* Pushed by pusha */
    uint32_t int_no, err_code;                          /* Interrupt number and error code */
    uint32_t eip, cs, eflags, useresp, ss;             /* Pushed by the processor automatically */
};

/* Function prototypes */
void syscall_init(void);
void syscall_handler(struct syscall_regs *regs);

/* System call implementations */
int sys_exit(int status);
int sys_write(int fd, const void *buf, size_t count);
int sys_read(int fd, void *buf, size_t count);
int sys_open(const char *filename, int flags, int mode);
int sys_close(int fd);
int sys_getpid(void);
int sys_sleep(uint32_t milliseconds);

/* User-space system call interface (inline assembly) */
static inline int syscall0(int num) {
    int ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a" (ret)
        : "a" (num)
    );
    return ret;
}

static inline int syscall1(int num, int arg1) {
    int ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a" (ret)
        : "a" (num), "b" (arg1)
    );
    return ret;
}

static inline int syscall2(int num, int arg1, int arg2) {
    int ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a" (ret)
        : "a" (num), "b" (arg1), "c" (arg2)
    );
    return ret;
}

static inline int syscall3(int num, int arg1, int arg2, int arg3) {
    int ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a" (ret)
        : "a" (num), "b" (arg1), "c" (arg2), "d" (arg3)
    );
    return ret;
}

/* User-friendly wrappers */
#define exit(status)        syscall1(SYS_EXIT, (int)(status))
#define write(fd, buf, cnt) syscall3(SYS_WRITE, (int)(fd), (int)(buf), (int)(cnt))
#define read(fd, buf, cnt)  syscall3(SYS_READ, (int)(fd), (int)(buf), (int)(cnt))
#define getpid()           syscall0(SYS_GETPID)
#define sleep(ms)          syscall1(SYS_SLEEP, (int)(ms))

#endif /* SYSCALL_H */