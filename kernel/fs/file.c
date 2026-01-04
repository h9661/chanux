/**
 * =============================================================================
 * Chanux OS - File Descriptor Management
 * =============================================================================
 * Implements per-process file descriptor tables and open file management.
 *
 * Design:
 *   - Each process has its own fd_table mapping fd numbers to file_t pointers
 *   - file_t represents an open file instance (system-wide)
 *   - Multiple fds can point to the same file_t (via dup/fork)
 *   - Reference counting on file_t for proper cleanup
 * =============================================================================
 */

#include "../include/fs/file.h"
#include "../include/fs/vfs.h"
#include "../include/mm/heap.h"
#include "../include/kernel.h"
#include "../drivers/vga/vga.h"

/* Maximum number of open files system-wide */
#define MAX_OPEN_FILES  256

/* System-wide open file table */
static file_t open_file_table[MAX_OPEN_FILES];
static bool file_table_initialized = false;

/* Console file entries for stdin/stdout/stderr */
static file_t console_stdin;
static file_t console_stdout;
static file_t console_stderr;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * Initialize the system-wide file table.
 */
static void init_file_table(void) {
    if (file_table_initialized) {
        return;
    }

    /* Clear open file table */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        open_file_table[i].ref_count = 0;
        open_file_table[i].flags = 0;
        open_file_table[i].offset = 0;
        open_file_table[i].inode = 0;
        open_file_table[i].type = 0;
        open_file_table[i].vnode = NULL;
    }

    /* Initialize console file entries */
    console_stdin.ref_count = 1;  /* Always referenced */
    console_stdin.flags = O_RDONLY;
    console_stdin.offset = 0;
    console_stdin.inode = 0;
    console_stdin.type = FILE_TYPE_CONSOLE;
    console_stdin.vnode = NULL;

    console_stdout.ref_count = 1;
    console_stdout.flags = O_WRONLY;
    console_stdout.offset = 0;
    console_stdout.inode = 0;
    console_stdout.type = FILE_TYPE_CONSOLE;
    console_stdout.vnode = NULL;

    console_stderr.ref_count = 1;
    console_stderr.flags = O_WRONLY;
    console_stderr.offset = 0;
    console_stderr.inode = 0;
    console_stderr.type = FILE_TYPE_CONSOLE;
    console_stderr.vnode = NULL;

    file_table_initialized = true;
}

/* =============================================================================
 * File Descriptor Table Management
 * =============================================================================
 */

/**
 * Create a new file descriptor table.
 *
 * Returns NULL on allocation failure.
 */
fd_table_t* fd_table_create(void) {
    /* Ensure file table is initialized */
    init_file_table();

    fd_table_t* table = (fd_table_t*)kmalloc(sizeof(fd_table_t));
    if (!table) {
        return NULL;
    }

    /* Clear all entries */
    for (int i = 0; i < MAX_FD_PER_PROCESS; i++) {
        table->entries[i] = NULL;
    }
    table->num_open = 0;

    return table;
}

/**
 * Destroy a file descriptor table.
 *
 * Closes all open file descriptors and frees the table.
 */
void fd_table_destroy(fd_table_t* table) {
    if (!table) {
        return;
    }

    /* Close all open files */
    for (int i = 0; i < MAX_FD_PER_PROCESS; i++) {
        if (table->entries[i]) {
            file_unref(table->entries[i]);
            table->entries[i] = NULL;
        }
    }

    kfree(table);
}

/**
 * Clone a file descriptor table (for fork).
 *
 * Creates a new table with the same entries, incrementing reference counts.
 */
fd_table_t* fd_table_clone(fd_table_t* src) {
    if (!src) {
        return NULL;
    }

    fd_table_t* dst = fd_table_create();
    if (!dst) {
        return NULL;
    }

    /* Copy entries and increment reference counts */
    for (int i = 0; i < MAX_FD_PER_PROCESS; i++) {
        if (src->entries[i]) {
            dst->entries[i] = src->entries[i];
            file_ref(dst->entries[i]);
        }
    }
    dst->num_open = src->num_open;

    return dst;
}

/* =============================================================================
 * File Descriptor Operations
 * =============================================================================
 */

/**
 * Allocate the lowest available file descriptor.
 *
 * Returns the fd number, or -1 if no fds available.
 */
int fd_alloc(fd_table_t* table) {
    if (!table) {
        return -1;
    }

    /* Find lowest available fd */
    for (int i = 0; i < MAX_FD_PER_PROCESS; i++) {
        if (table->entries[i] == NULL) {
            return i;
        }
    }

    return -1;  /* No available fds */
}

/**
 * Free a file descriptor.
 *
 * Decrements reference count on the underlying file.
 */
void fd_free(fd_table_t* table, int fd) {
    if (!table || fd < 0 || fd >= MAX_FD_PER_PROCESS) {
        return;
    }

    if (table->entries[fd]) {
        file_unref(table->entries[fd]);
        table->entries[fd] = NULL;
        if (table->num_open > 0) {
            table->num_open--;
        }
    }
}

/**
 * Get the file_t for a file descriptor.
 *
 * Returns NULL if fd is invalid or not open.
 */
file_t* fd_get_file(fd_table_t* table, int fd) {
    if (!table || fd < 0 || fd >= MAX_FD_PER_PROCESS) {
        return NULL;
    }

    return table->entries[fd];
}

/**
 * Set the file_t for a file descriptor.
 *
 * Returns 0 on success, -1 on error.
 */
int fd_set_file(fd_table_t* table, int fd, file_t* file) {
    if (!table || fd < 0 || fd >= MAX_FD_PER_PROCESS) {
        return -1;
    }

    /* If there was an old file, unreference it */
    if (table->entries[fd]) {
        file_unref(table->entries[fd]);
        table->num_open--;
    }

    table->entries[fd] = file;
    if (file) {
        table->num_open++;
    }

    return 0;
}

/* =============================================================================
 * Open File Table (System-wide)
 * =============================================================================
 */

/**
 * Allocate a new file_t from the system-wide table.
 *
 * Returns NULL if no slots available.
 */
file_t* file_alloc(void) {
    init_file_table();

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_file_table[i].ref_count == 0) {
            file_t* file = &open_file_table[i];
            file->ref_count = 1;
            file->flags = 0;
            file->offset = 0;
            file->inode = 0;
            file->type = FILE_TYPE_REGULAR;
            file->vnode = NULL;
            return file;
        }
    }

    return NULL;  /* No available slots */
}

/**
 * Free a file_t back to the system-wide table.
 *
 * Only call this directly if you know ref_count is 0.
 * Normally use file_unref() instead.
 */
void file_free(file_t* file) {
    if (!file) {
        return;
    }

    /* Don't free console files */
    if (file == &console_stdin || file == &console_stdout || file == &console_stderr) {
        return;
    }

    /* Clean up vnode reference */
    if (file->vnode) {
        vnode_unref(file->vnode);
        file->vnode = NULL;
    }

    file->ref_count = 0;
    file->flags = 0;
    file->offset = 0;
    file->inode = 0;
    file->type = 0;
}

/**
 * Increment reference count on a file.
 */
void file_ref(file_t* file) {
    if (file) {
        file->ref_count++;
    }
}

/**
 * Decrement reference count on a file.
 *
 * Frees the file if ref_count reaches 0.
 */
void file_unref(file_t* file) {
    if (!file) {
        return;
    }

    /* Console files are never freed */
    if (file == &console_stdin || file == &console_stdout || file == &console_stderr) {
        return;
    }

    if (file->ref_count > 0) {
        file->ref_count--;
        if (file->ref_count == 0) {
            file_free(file);
        }
    }
}

/* =============================================================================
 * Standard I/O Initialization
 * =============================================================================
 */

/**
 * Initialize standard file descriptors (stdin, stdout, stderr) for a process.
 *
 * Sets up fd 0, 1, 2 to point to console files.
 * Returns 0 on success, -1 on error.
 */
int fd_init_stdio(fd_table_t* table) {
    if (!table) {
        return -1;
    }

    /* Initialize file table if needed */
    init_file_table();

    /* Set up stdin (fd 0) */
    table->entries[FD_STDIN] = &console_stdin;

    /* Set up stdout (fd 1) */
    table->entries[FD_STDOUT] = &console_stdout;

    /* Set up stderr (fd 2) */
    table->entries[FD_STDERR] = &console_stderr;

    table->num_open = 3;

    return 0;
}

/**
 * Check if a file_t is a console file (stdin/stdout/stderr).
 */
bool file_is_console(file_t* file) {
    return file == &console_stdin ||
           file == &console_stdout ||
           file == &console_stderr;
}

/**
 * Get the console file for a standard fd.
 */
file_t* file_get_console(int fd) {
    switch (fd) {
        case FD_STDIN:  return &console_stdin;
        case FD_STDOUT: return &console_stdout;
        case FD_STDERR: return &console_stderr;
        default:        return NULL;
    }
}
