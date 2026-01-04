/**
 * =============================================================================
 * Chanux OS - System Call Interface
 * =============================================================================
 * Defines the system call numbers, dispatcher, and kernel-side syscall API.
 *
 * System Call Mechanism:
 *   - Uses x86_64 SYSCALL/SYSRET instructions for fast user/kernel transitions
 *   - Syscall number passed in RAX
 *   - Arguments passed in RDI, RSI, RDX, R10, R8, R9 (note: R10 replaces RCX)
 *   - Return value in RAX (negative values indicate errors)
 *
 * MSR Configuration:
 *   - IA32_STAR (0xC0000081): Segment selectors
 *   - IA32_LSTAR (0xC0000082): Syscall entry point (64-bit)
 *   - IA32_FMASK (0xC0000084): RFLAGS mask (clear IF on entry)
 * =============================================================================
 */

#ifndef CHANUX_SYSCALL_H
#define CHANUX_SYSCALL_H

#include "../types.h"

/* =============================================================================
 * System Call Numbers
 * =============================================================================
 */

#define SYS_EXIT        0       /* void exit(int code) */
#define SYS_WRITE       1       /* ssize_t write(int fd, const void* buf, size_t len) */
#define SYS_READ        2       /* ssize_t read(int fd, void* buf, size_t len) */
#define SYS_YIELD       3       /* int yield(void) */
#define SYS_GETPID      4       /* pid_t getpid(void) */
#define SYS_SLEEP       5       /* int sleep(uint64_t ms) */

/* Phase 6: File system syscalls */
#define SYS_OPEN        6       /* int open(const char* path, int flags) */
#define SYS_CLOSE       7       /* int close(int fd) */
#define SYS_LSEEK       8       /* off_t lseek(int fd, off_t offset, int whence) */
#define SYS_STAT        9       /* int stat(const char* path, struct stat* buf) */
#define SYS_FSTAT       10      /* int fstat(int fd, struct stat* buf) */
#define SYS_READDIR     11      /* int readdir(int fd, struct dirent* entry, int index) */
#define SYS_GETCWD      12      /* int getcwd(char* buf, size_t size) */
#define SYS_CHDIR       13      /* int chdir(const char* path) */

#define SYS_MAX         14      /* Number of system calls */

/* =============================================================================
 * Error Codes (negative return values)
 * =============================================================================
 */

#define ENOSYS          38      /* Invalid system call number */
#define EFAULT          14      /* Bad address (invalid user pointer) */
#define EBADF           9       /* Bad file descriptor */
#define EINVAL          22      /* Invalid argument */
#define EINTR           4       /* Interrupted system call */
#define ENOMEM          12      /* Out of memory */

/* Phase 6: Additional error codes */
#define ENOENT          2       /* No such file or directory */
#define EIO             5       /* I/O error */
#define EACCES          13      /* Permission denied */
#define EEXIST          17      /* File exists */
#define ENOTDIR         20      /* Not a directory */
#define EISDIR          21      /* Is a directory */
#define EMFILE          24      /* Too many open files */
#define ENOSPC          28      /* No space left on device */
#define ERANGE          34      /* Result too large */
#define ENAMETOOLONG    36      /* File name too long */

/* =============================================================================
 * MSR Addresses
 * =============================================================================
 */

#define MSR_EFER        0xC0000080      /* Extended Feature Enable Register */
#define MSR_STAR        0xC0000081      /* Segment selectors for SYSCALL/SYSRET */
#define MSR_LSTAR       0xC0000082      /* 64-bit syscall entry point */
#define MSR_CSTAR       0xC0000083      /* Compatibility mode entry (unused) */
#define MSR_SFMASK      0xC0000084      /* RFLAGS mask for SYSCALL */

/* EFER bits */
#define EFER_SCE        (1 << 0)        /* System Call Enable */
#define EFER_LME        (1 << 8)        /* Long Mode Enable */
#define EFER_NXE        (1 << 11)       /* No-Execute Enable */

/* RFLAGS bits to mask on syscall entry */
#define SYSCALL_RFLAGS_MASK     0x200   /* Clear IF (disable interrupts) */

/* =============================================================================
 * Syscall Frame
 * =============================================================================
 * Structure representing saved state during a system call.
 */

typedef struct {
    /* Callee-saved registers (pushed by syscall_entry) */
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;

    /* Syscall return info (pushed by syscall_entry) */
    uint64_t rcx;           /* User RIP (saved by SYSCALL instruction) */
    uint64_t r11;           /* User RFLAGS (saved by SYSCALL instruction) */

    /* User stack pointer (saved by syscall_entry) */
    uint64_t user_rsp;
} syscall_frame_t;

/* =============================================================================
 * Syscall Handler Function Type
 * =============================================================================
 */

typedef int64_t (*syscall_fn_t)(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                                 uint64_t arg4, uint64_t arg5, uint64_t arg6);

/* =============================================================================
 * Syscall API
 * =============================================================================
 */

/**
 * Initialize the system call subsystem.
 * Configures MSRs for SYSCALL/SYSRET and registers the syscall handler.
 */
void syscall_init(void);

/**
 * System call dispatcher.
 * Called from assembly after saving registers.
 *
 * @param num  Syscall number (from RAX)
 * @param arg1-arg6  Syscall arguments
 * @return     Return value for syscall (placed in RAX)
 */
int64_t syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2,
                         uint64_t arg3, uint64_t arg4, uint64_t arg5);

/* =============================================================================
 * Individual System Call Handlers
 * =============================================================================
 */

/* Process control */
NORETURN void sys_exit(int code);
int64_t sys_yield(void);
int64_t sys_getpid(void);
int64_t sys_sleep(uint64_t ms);

/* I/O operations */
int64_t sys_write(int fd, const void* buf, size_t len);
int64_t sys_read(int fd, void* buf, size_t len);

/* File system operations (Phase 6) */
int64_t sys_open(const char* path, int flags);
int64_t sys_close(int fd);
int64_t sys_lseek(int fd, int64_t offset, int whence);
int64_t sys_stat(const char* path, void* buf);
int64_t sys_fstat(int fd, void* buf);
int64_t sys_readdir(int fd, void* entry, int index);
int64_t sys_getcwd(char* buf, size_t size);
int64_t sys_chdir(const char* path);

/* =============================================================================
 * Assembly Functions (defined in syscall.asm)
 * =============================================================================
 */

/**
 * Syscall entry point (called by SYSCALL instruction).
 * Saves registers and calls syscall_dispatch().
 */
extern void syscall_entry(void);

#endif /* CHANUX_SYSCALL_H */
