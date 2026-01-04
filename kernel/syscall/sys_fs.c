/**
 * =============================================================================
 * Chanux OS - Filesystem System Calls
 * =============================================================================
 * Implements system calls for filesystem operations:
 *   - sys_open:    Open a file
 *   - sys_close:   Close a file descriptor
 *   - sys_lseek:   Seek to position in file
 *   - sys_stat:    Get file status by path
 *   - sys_fstat:   Get file status by fd
 *   - sys_readdir: Read directory entry
 *   - sys_getcwd:  Get current working directory
 *   - sys_chdir:   Change current working directory
 * =============================================================================
 */

#include "syscall/syscall.h"
#include "fs/vfs.h"
#include "fs/file.h"
#include "fs/ramfs.h"
#include "proc/process.h"
#include "kernel.h"
#include "string.h"

/* =============================================================================
 * User Pointer Validation
 * =============================================================================
 */

/* User space address limit (canonical lower half) */
#define USER_SPACE_END  0x0000800000000000ULL

/**
 * Validate a user-space pointer for reading.
 */
static bool validate_user_ptr_read(const void* ptr, size_t len) {
    uintptr_t addr = (uintptr_t)ptr;

    if (ptr == NULL || len == 0) {
        return false;
    }
    if (addr >= USER_SPACE_END) {
        return false;
    }
    if (addr + len < addr) {
        return false;
    }
    if (addr + len > USER_SPACE_END) {
        return false;
    }

    return true;
}

/**
 * Validate a user-space pointer for writing.
 */
static bool validate_user_ptr_write(void* ptr, size_t len) {
    return validate_user_ptr_read(ptr, len);
}

/**
 * Validate a user-space string (null-terminated).
 */
static bool validate_user_string(const char* str, size_t max_len) {
    if (str == NULL) {
        return false;
    }

    uintptr_t addr = (uintptr_t)str;
    if (addr >= USER_SPACE_END) {
        return false;
    }

    /* Check each character up to max_len */
    for (size_t i = 0; i < max_len; i++) {
        if (addr + i >= USER_SPACE_END) {
            return false;
        }
        if (str[i] == '\0') {
            return true;
        }
    }

    /* String too long */
    return false;
}

/* =============================================================================
 * sys_open - Open a File
 * =============================================================================
 */

/**
 * Open a file and return a file descriptor.
 *
 * @param path  Path to the file (user space)
 * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT)
 * @return      File descriptor on success, negative error on failure
 */
int64_t sys_open(const char* path, int flags) {
    /* Validate path */
    if (!validate_user_string(path, VFS_MAX_PATH)) {
        return -EFAULT;
    }

    /* Get current process */
    process_t* proc = process_current();
    if (!proc || !proc->fd_table) {
        return -ENOMEM;
    }

    /* Resolve path to absolute */
    char abs_path[VFS_MAX_PATH];
    if (path_normalize(path, proc->cwd, abs_path, VFS_MAX_PATH) < 0) {
        return -ENAMETOOLONG;
    }

    /* Allocate a file descriptor */
    int fd = fd_alloc(proc->fd_table);
    if (fd < 0) {
        return -EMFILE;
    }

    /* Open the file via VFS */
    file_t* file = NULL;
    int result = vfs_open(abs_path, (uint32_t)flags, &file);
    if (result < 0) {
        fd_free(proc->fd_table, fd);
        return result;
    }

    /* Install file in fd table */
    proc->fd_table->entries[fd] = file;

    return fd;
}

/* =============================================================================
 * sys_close - Close a File Descriptor
 * =============================================================================
 */

/**
 * Close an open file descriptor.
 *
 * @param fd File descriptor to close
 * @return   0 on success, negative error on failure
 */
int64_t sys_close(int fd) {
    /* Get current process */
    process_t* proc = process_current();
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }

    /* Validate fd */
    if (fd < 0 || fd >= MAX_FD_PER_PROCESS) {
        return -EBADF;
    }

    file_t* file = proc->fd_table->entries[fd];
    if (!file) {
        return -EBADF;
    }

    /* Don't allow closing stdin/stdout/stderr via this syscall */
    if (fd < 3) {
        return -EINVAL;
    }

    /* Close the file via VFS */
    int result = vfs_close(file);

    /* Remove from fd table */
    proc->fd_table->entries[fd] = NULL;

    return result;
}

/* =============================================================================
 * sys_lseek - Seek to Position in File
 * =============================================================================
 */

/**
 * Reposition read/write file offset.
 *
 * @param fd     File descriptor
 * @param offset Offset in bytes
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return       New offset on success, negative error on failure
 */
int64_t sys_lseek(int fd, int64_t offset, int whence) {
    /* Get current process */
    process_t* proc = process_current();
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }

    /* Validate fd */
    if (fd < 0 || fd >= MAX_FD_PER_PROCESS) {
        return -EBADF;
    }

    file_t* file = proc->fd_table->entries[fd];
    if (!file) {
        return -EBADF;
    }

    /* Perform seek via VFS */
    return vfs_lseek(file, offset, whence);
}

/* =============================================================================
 * sys_stat - Get File Status by Path
 * =============================================================================
 */

/**
 * Get file status by path.
 *
 * @param path Path to the file (user space)
 * @param buf  Pointer to stat structure (user space)
 * @return     0 on success, negative error on failure
 */
int64_t sys_stat(const char* path, void* buf) {
    /* Validate path */
    if (!validate_user_string(path, VFS_MAX_PATH)) {
        return -EFAULT;
    }

    /* Validate buffer */
    if (!validate_user_ptr_write(buf, sizeof(stat_t))) {
        return -EFAULT;
    }

    /* Get current process */
    process_t* proc = process_current();
    if (!proc) {
        return -ENOMEM;
    }

    /* Resolve path to absolute */
    char abs_path[VFS_MAX_PATH];
    if (path_normalize(path, proc->cwd, abs_path, VFS_MAX_PATH) < 0) {
        return -ENAMETOOLONG;
    }

    /* Get status via VFS */
    return vfs_stat(abs_path, (stat_t*)buf);
}

/* =============================================================================
 * sys_fstat - Get File Status by File Descriptor
 * =============================================================================
 */

/**
 * Get file status by file descriptor.
 *
 * @param fd  File descriptor
 * @param buf Pointer to stat structure (user space)
 * @return    0 on success, negative error on failure
 */
int64_t sys_fstat(int fd, void* buf) {
    /* Validate buffer */
    if (!validate_user_ptr_write(buf, sizeof(stat_t))) {
        return -EFAULT;
    }

    /* Get current process */
    process_t* proc = process_current();
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }

    /* Validate fd */
    if (fd < 0 || fd >= MAX_FD_PER_PROCESS) {
        return -EBADF;
    }

    file_t* file = proc->fd_table->entries[fd];
    if (!file || !file->vnode) {
        return -EBADF;
    }

    /* Fill stat structure from vnode */
    stat_t* st = (stat_t*)buf;
    vnode_t* vn = file->vnode;

    st->st_mode = (vn->type == INODE_TYPE_DIR) ? S_IFDIR : S_IFREG;
    st->st_size = vn->inode ? vn->inode->size : 0;
    st->st_ino = vn->inode_num;
    st->st_nlink = 1;  /* Simplified */
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_blksize = RAMFS_BLOCK_SIZE;
    st->st_blocks = (st->st_size + RAMFS_BLOCK_SIZE - 1) / RAMFS_BLOCK_SIZE;

    return 0;
}

/* =============================================================================
 * sys_readdir - Read Directory Entry
 * =============================================================================
 */

/**
 * Read a directory entry by index.
 *
 * @param fd    File descriptor of open directory
 * @param entry Pointer to dirent structure (user space)
 * @param index Entry index to read
 * @return      0 on success, -ENOENT if no more entries, negative error on failure
 */
int64_t sys_readdir(int fd, void* entry, int index) {
    /* Validate buffer */
    if (!validate_user_ptr_write(entry, sizeof(ramfs_dirent_t))) {
        return -EFAULT;
    }

    /* Get current process */
    process_t* proc = process_current();
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }

    /* Validate fd */
    if (fd < 0 || fd >= MAX_FD_PER_PROCESS) {
        return -EBADF;
    }

    file_t* file = proc->fd_table->entries[fd];
    if (!file) {
        return -EBADF;
    }

    /* Read directory entry via VFS */
    return vfs_readdir(file, (ramfs_dirent_t*)entry, (uint32_t)index);
}

/* =============================================================================
 * sys_getcwd - Get Current Working Directory
 * =============================================================================
 */

/**
 * Get current working directory.
 *
 * @param buf  Buffer to store path (user space)
 * @param size Size of buffer
 * @return     0 on success, negative error on failure
 */
int64_t sys_getcwd(char* buf, size_t size) {
    /* Validate buffer */
    if (!validate_user_ptr_write(buf, size)) {
        return -EFAULT;
    }

    if (size == 0) {
        return -EINVAL;
    }

    /* Get current process */
    process_t* proc = process_current();
    if (!proc) {
        return -ENOMEM;
    }

    /* Copy cwd to buffer */
    size_t cwd_len = strlen(proc->cwd);
    if (cwd_len >= size) {
        return -ERANGE;
    }

    memcpy(buf, proc->cwd, cwd_len + 1);
    return 0;
}

/* =============================================================================
 * sys_chdir - Change Current Working Directory
 * =============================================================================
 */

/**
 * Change current working directory.
 *
 * @param path Path to new directory (user space)
 * @return     0 on success, negative error on failure
 */
int64_t sys_chdir(const char* path) {
    /* Validate path */
    if (!validate_user_string(path, VFS_MAX_PATH)) {
        return -EFAULT;
    }

    /* Get current process */
    process_t* proc = process_current();
    if (!proc) {
        return -ENOMEM;
    }

    /* Resolve path to absolute */
    char abs_path[VFS_MAX_PATH];
    if (path_normalize(path, proc->cwd, abs_path, VFS_MAX_PATH) < 0) {
        return -ENAMETOOLONG;
    }

    /* Verify path exists and is a directory */
    stat_t st;
    int result = vfs_stat(abs_path, &st);
    if (result < 0) {
        return result;
    }

    if (st.st_mode != S_IFDIR) {
        return -ENOTDIR;
    }

    /* Update process cwd */
    size_t len = strlen(abs_path);
    if (len >= CWD_MAX) {
        return -ENAMETOOLONG;
    }

    memcpy(proc->cwd, abs_path, len + 1);
    return 0;
}
