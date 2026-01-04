/*
 * file.h - File descriptor table structures
 *
 * Per-process file descriptor management for Chanux OS.
 */

#ifndef _KERNEL_FS_FILE_H
#define _KERNEL_FS_FILE_H

#include "../types.h"

/* Maximum open files per process */
#define MAX_FD_PER_PROCESS  16

/* Standard file descriptors */
#define FD_STDIN    0
#define FD_STDOUT   1
#define FD_STDERR   2

/* File open flags */
#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      0x0003
#define O_ACCMODE   0x0003  /* Mask for access mode */
#define O_CREAT     0x0100
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_EXCL      0x0800

/* Seek origins */
#define SEEK_SET    0   /* Seek from beginning of file */
#define SEEK_CUR    1   /* Seek from current position */
#define SEEK_END    2   /* Seek from end of file */

/* File types for special handling */
#define FILE_TYPE_REGULAR   0   /* Regular file */
#define FILE_TYPE_DIR       1   /* Directory */
#define FILE_TYPE_CONSOLE   2   /* Console (stdin/stdout/stderr) */

/* Forward declarations */
struct vnode;

/*
 * Open file entry (system-wide table)
 *
 * Represents an open file instance. Multiple file descriptors
 * can point to the same file_t (via dup/fork).
 */
typedef struct file {
    uint32_t        ref_count;      /* Reference count */
    uint32_t        flags;          /* Open flags (O_*) */
    uint64_t        offset;         /* Current file position */
    uint32_t        inode;          /* Inode number (0 for special files) */
    uint32_t        type;           /* FILE_TYPE_* */
    struct vnode*   vnode;          /* VFS node pointer (NULL for console) */
} file_t;

/*
 * Per-process file descriptor table
 *
 * Each process has its own fd table mapping fd numbers to file_t pointers.
 */
typedef struct fd_table {
    file_t*     entries[MAX_FD_PER_PROCESS];    /* File descriptor entries */
    uint32_t    num_open;                        /* Count of open descriptors */
} fd_table_t;

/* File descriptor table management */
fd_table_t* fd_table_create(void);
void fd_table_destroy(fd_table_t* table);
fd_table_t* fd_table_clone(fd_table_t* src);    /* For fork */

/* File descriptor operations */
int fd_alloc(fd_table_t* table);                 /* Allocate lowest available fd */
void fd_free(fd_table_t* table, int fd);         /* Free a file descriptor */
file_t* fd_get_file(fd_table_t* table, int fd);  /* Get file from fd */
int fd_set_file(fd_table_t* table, int fd, file_t* file);

/* Open file table (system-wide) */
file_t* file_alloc(void);
void file_free(file_t* file);
void file_ref(file_t* file);        /* Increment reference count */
void file_unref(file_t* file);      /* Decrement reference count, free if 0 */

/* Standard I/O initialization */
int fd_init_stdio(fd_table_t* table);  /* Set up fd 0, 1, 2 for console */

#endif /* _KERNEL_FS_FILE_H */
