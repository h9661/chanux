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

/* Phase 6: File system syscalls */
#define SYS_OPEN        6       /* int open(const char* path, int flags) */
#define SYS_CLOSE       7       /* int close(int fd) */
#define SYS_LSEEK       8       /* off_t lseek(int fd, off_t offset, int whence) */
#define SYS_STAT        9       /* int stat(const char* path, struct stat* buf) */
#define SYS_FSTAT       10      /* int fstat(int fd, struct stat* buf) */
#define SYS_READDIR     11      /* int readdir(int fd, struct dirent* entry, int index) */
#define SYS_GETCWD      12      /* int getcwd(char* buf, size_t size) */
#define SYS_CHDIR       13      /* int chdir(const char* path) */

/* =============================================================================
 * File Open Flags
 * =============================================================================
 */

#define O_RDONLY        0x0001  /* Open for reading only */
#define O_WRONLY        0x0002  /* Open for writing only */
#define O_RDWR          0x0003  /* Open for reading and writing */
#define O_CREAT         0x0100  /* Create file if not exists */
#define O_TRUNC         0x0200  /* Truncate file to zero length */
#define O_APPEND        0x0400  /* Append on each write */

/* lseek whence values */
#define SEEK_SET        0       /* Offset from beginning of file */
#define SEEK_CUR        1       /* Offset from current position */
#define SEEK_END        2       /* Offset from end of file */

/* =============================================================================
 * File Types (for stat)
 * =============================================================================
 */

#define S_IFREG         1       /* Regular file */
#define S_IFDIR         2       /* Directory */

/* =============================================================================
 * File System Structures
 * =============================================================================
 */

/* Maximum filename length */
#define NAME_MAX        255

/* File status structure (simplified) */
typedef struct stat {
    uint32_t st_mode;           /* File type and permissions */
    uint64_t st_size;           /* Total size in bytes */
    uint64_t st_ino;            /* Inode number */
    uint32_t st_nlink;          /* Number of hard links */
    uint32_t st_uid;            /* Owner user ID */
    uint32_t st_gid;            /* Owner group ID */
    uint32_t st_blksize;        /* Block size for I/O */
    uint64_t st_blocks;         /* Number of blocks allocated */
} stat_t;

/* Directory entry structure */
typedef struct dirent {
    uint32_t d_ino;             /* Inode number */
    uint32_t d_type;            /* File type */
    char     d_name[256];       /* Filename */
} dirent_t;

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
 * File System Functions (Phase 6)
 * =============================================================================
 */

/**
 * Open a file.
 *
 * @param path Path to the file
 * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT)
 * @return File descriptor on success, negative error on failure
 */
int open(const char* path, int flags);

/**
 * Close a file descriptor.
 *
 * @param fd File descriptor to close
 * @return 0 on success, negative error on failure
 */
int close(int fd);

/**
 * Seek to position in file.
 *
 * @param fd File descriptor
 * @param offset Offset in bytes
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return New position on success, negative error on failure
 */
ssize_t lseek(int fd, ssize_t offset, int whence);

/**
 * Get file status by path.
 *
 * @param path Path to the file
 * @param buf Pointer to stat structure
 * @return 0 on success, negative error on failure
 */
int stat(const char* path, stat_t* buf);

/**
 * Get file status by file descriptor.
 *
 * @param fd File descriptor
 * @param buf Pointer to stat structure
 * @return 0 on success, negative error on failure
 */
int fstat(int fd, stat_t* buf);

/**
 * Read directory entry.
 *
 * @param fd File descriptor of open directory
 * @param entry Pointer to dirent structure
 * @param index Entry index to read
 * @return 0 on success, -1 if no more entries, negative error on failure
 */
int readdir_r(int fd, dirent_t* entry, int index);

/**
 * Get current working directory.
 *
 * @param buf Buffer to store path
 * @param size Size of buffer
 * @return Pointer to buf on success, NULL on failure
 */
char* getcwd(char* buf, size_t size);

/**
 * Change current working directory.
 *
 * @param path Path to new directory
 * @return 0 on success, negative error on failure
 */
int chdir(const char* path);

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
