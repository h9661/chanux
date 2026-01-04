/**
 * =============================================================================
 * Chanux OS - User Space C Library
 * =============================================================================
 * Provides C wrapper functions for system calls.
 * This is a minimal libc for Chanux user programs.
 * =============================================================================
 */

#include "../include/syscall.h"

/* =============================================================================
 * System Call Wrappers
 * =============================================================================
 */

/**
 * Terminate the current process.
 */
void exit(int code) {
    syscall1(SYS_EXIT, code);

    /* Should never reach here, but satisfy the noreturn attribute */
    for (;;) {
        /* Loop forever */
    }
}

/**
 * Write data to a file descriptor.
 */
ssize_t write(int fd, const void* buf, size_t len) {
    return (ssize_t)syscall3(SYS_WRITE, fd, buf, len);
}

/**
 * Read data from a file descriptor.
 */
ssize_t read(int fd, void* buf, size_t len) {
    return (ssize_t)syscall3(SYS_READ, fd, buf, len);
}

/**
 * Yield CPU to another process.
 */
int yield(void) {
    return (int)syscall0(SYS_YIELD);
}

/**
 * Get current process ID.
 */
pid_t getpid(void) {
    return (pid_t)syscall0(SYS_GETPID);
}

/**
 * Sleep for specified milliseconds.
 */
int sleep(uint64_t ms) {
    return (int)syscall1(SYS_SLEEP, ms);
}

/* =============================================================================
 * File System Wrappers (Phase 6)
 * =============================================================================
 */

/**
 * Open a file.
 */
int open(const char* path, int flags) {
    return (int)syscall2(SYS_OPEN, path, flags);
}

/**
 * Close a file descriptor.
 */
int close(int fd) {
    return (int)syscall1(SYS_CLOSE, fd);
}

/**
 * Seek to position in file.
 */
ssize_t lseek(int fd, ssize_t offset, int whence) {
    return (ssize_t)syscall3(SYS_LSEEK, fd, offset, whence);
}

/**
 * Get file status by path.
 */
int stat(const char* path, stat_t* buf) {
    return (int)syscall2(SYS_STAT, path, buf);
}

/**
 * Get file status by file descriptor.
 */
int fstat(int fd, stat_t* buf) {
    return (int)syscall2(SYS_FSTAT, fd, buf);
}

/**
 * Read directory entry.
 */
int readdir_r(int fd, dirent_t* entry, int index) {
    return (int)syscall3(SYS_READDIR, fd, entry, index);
}

/**
 * Get current working directory.
 */
char* getcwd(char* buf, size_t size) {
    int result = (int)syscall2(SYS_GETCWD, buf, size);
    if (result < 0) {
        return NULL;
    }
    return buf;
}

/**
 * Change current working directory.
 */
int chdir(const char* path) {
    return (int)syscall1(SYS_CHDIR, path);
}

/* =============================================================================
 * String Functions
 * =============================================================================
 */

/**
 * Calculate string length.
 */
size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}

/**
 * Print a string to stdout.
 */
ssize_t puts(const char* s) {
    size_t len = strlen(s);
    return write(1, s, len);
}

/* =============================================================================
 * Memory Functions
 * =============================================================================
 */

/**
 * Set memory to a value.
 */
void* memset(void* dest, int val, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    while (count--) {
        *d++ = (uint8_t)val;
    }
    return dest;
}

/**
 * Copy memory.
 */
void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

/* =============================================================================
 * Number to String Conversion
 * =============================================================================
 */

/**
 * Convert an unsigned integer to decimal string.
 * Returns pointer to the beginning of the string within buf.
 */
static char* utoa_internal(uint64_t value, char* buf, size_t buf_size) {
    if (buf_size == 0) return buf;

    char* p = buf + buf_size - 1;
    *p = '\0';

    if (value == 0) {
        *(--p) = '0';
        return p;
    }

    while (value > 0 && p > buf) {
        *(--p) = '0' + (value % 10);
        value /= 10;
    }

    return p;
}

/**
 * Print an unsigned integer to stdout.
 */
void print_uint(uint64_t value) {
    char buf[24];
    char* str = utoa_internal(value, buf, sizeof(buf));
    puts(str);
}

/**
 * Print a signed integer to stdout.
 */
void print_int(int64_t value) {
    if (value < 0) {
        write(1, "-", 1);
        value = -value;
    }
    print_uint((uint64_t)value);
}

/**
 * Print an unsigned integer in hexadecimal.
 */
void print_hex(uint64_t value) {
    static const char hex_chars[] = "0123456789abcdef";
    char buf[19];  /* "0x" + 16 hex digits + null */
    char* p = buf + 18;

    *p = '\0';

    if (value == 0) {
        *(--p) = '0';
    } else {
        while (value > 0 && p > buf + 2) {
            *(--p) = hex_chars[value & 0xF];
            value >>= 4;
        }
    }

    *(--p) = 'x';
    *(--p) = '0';

    puts(p);
}
