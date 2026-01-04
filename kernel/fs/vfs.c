/**
 * =============================================================================
 * Chanux OS - Virtual Filesystem Layer
 * =============================================================================
 * Provides a unified interface for filesystem operations.
 * Currently supports RAMFS only.
 * =============================================================================
 */

#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "fs/file.h"
#include "mm/heap.h"
#include "kernel.h"
#include "string.h"
#include "drivers/vga/vga.h"

/* Maximum number of vnodes */
#define MAX_VNODES  256

/* Global root vnode */
vnode_t* g_root_vnode = NULL;

/* Vnode table */
static vnode_t vnode_table[MAX_VNODES];
static bool vfs_initialized = false;

/* Forward declarations for RAMFS VFS ops */
static int64_t ramfs_vfs_read(vnode_t* vn, void* buf, size_t count, uint64_t offset);
static int64_t ramfs_vfs_write(vnode_t* vn, const void* buf, size_t count, uint64_t offset);
static int ramfs_vfs_lookup(vnode_t* dir, const char* name, vnode_t** result);
static int ramfs_vfs_create(vnode_t* dir, const char* name, uint32_t type, vnode_t** result);
static int ramfs_vfs_unlink(vnode_t* dir, const char* name);
static int ramfs_vfs_readdir(vnode_t* dir, uint32_t index, ramfs_dirent_t* entry);
static int ramfs_vfs_stat(vnode_t* vn, stat_t* buf);
static int ramfs_vfs_truncate(vnode_t* vn, uint64_t size);

/* RAMFS VFS operations */
static vfs_ops_t ramfs_vfs_ops = {
    .read = ramfs_vfs_read,
    .write = ramfs_vfs_write,
    .lookup = ramfs_vfs_lookup,
    .create = ramfs_vfs_create,
    .unlink = ramfs_vfs_unlink,
    .readdir = ramfs_vfs_readdir,
    .stat = ramfs_vfs_stat,
    .truncate = ramfs_vfs_truncate,
};

/* =============================================================================
 * Vnode Management
 * =============================================================================
 */

/**
 * Initialize the VFS.
 */
void vfs_init(void) {
    if (vfs_initialized) {
        return;
    }

    /* Clear vnode table */
    for (int i = 0; i < MAX_VNODES; i++) {
        vnode_table[i].ref_count = 0;
        vnode_table[i].inode = NULL;
    }

    /* Initialize RAMFS */
    if (ramfs_init() < 0) {
        PANIC("Failed to initialize RAMFS");
    }

    /* Create root vnode */
    g_root_vnode = vnode_alloc();
    if (!g_root_vnode) {
        PANIC("Failed to allocate root vnode");
    }

    g_root_vnode->inode_num = RAMFS_ROOT_INODE;
    g_root_vnode->type = INODE_TYPE_DIR;
    g_root_vnode->ref_count = 1;  /* Root is always referenced */
    g_root_vnode->inode = ramfs_get_inode(RAMFS_ROOT_INODE);
    g_root_vnode->ops = &ramfs_vfs_ops;
    g_root_vnode->fs_data = NULL;

    vfs_initialized = true;

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[VFS] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Virtual filesystem initialized\n");
}

/**
 * Allocate a vnode.
 */
vnode_t* vnode_alloc(void) {
    for (int i = 0; i < MAX_VNODES; i++) {
        if (vnode_table[i].ref_count == 0) {
            vnode_t* vn = &vnode_table[i];
            vn->ref_count = 1;
            vn->inode_num = 0;
            vn->type = 0;
            vn->inode = NULL;
            vn->ops = NULL;
            vn->fs_data = NULL;
            return vn;
        }
    }
    return NULL;
}

/**
 * Free a vnode.
 */
void vnode_free(vnode_t* vn) {
    if (!vn) return;

    /* Don't free root vnode */
    if (vn == g_root_vnode) return;

    vn->ref_count = 0;
    vn->inode = NULL;
    vn->ops = NULL;
}

/**
 * Increment vnode reference count.
 */
void vnode_ref(vnode_t* vn) {
    if (vn) {
        vn->ref_count++;
    }
}

/**
 * Decrement vnode reference count.
 */
void vnode_unref(vnode_t* vn) {
    if (!vn) return;

    /* Don't free root vnode */
    if (vn == g_root_vnode) return;

    if (vn->ref_count > 0) {
        vn->ref_count--;
        if (vn->ref_count == 0) {
            vnode_free(vn);
        }
    }
}

/**
 * Get or create a vnode for an inode.
 */
static vnode_t* vnode_get_or_create(uint32_t inode_num) {
    /* Check if vnode already exists for this inode */
    for (int i = 0; i < MAX_VNODES; i++) {
        if (vnode_table[i].ref_count > 0 && vnode_table[i].inode_num == inode_num) {
            vnode_ref(&vnode_table[i]);
            return &vnode_table[i];
        }
    }

    /* Create new vnode */
    vnode_t* vn = vnode_alloc();
    if (!vn) {
        return NULL;
    }

    ramfs_inode_t* inode = ramfs_get_inode(inode_num);
    if (!inode) {
        vnode_free(vn);
        return NULL;
    }

    vn->inode_num = inode_num;
    vn->type = inode->type;
    vn->inode = inode;
    vn->ops = &ramfs_vfs_ops;

    return vn;
}

/* =============================================================================
 * Path Resolution
 * =============================================================================
 */

/**
 * Look up a path and return the vnode.
 *
 * Path can be absolute or relative (relative to root for now).
 * Returns 0 on success, -1 on error.
 */
int vfs_lookup(const char* path, vnode_t** result) {
    if (!path || !result || !vfs_initialized) {
        return -1;
    }

    /* Normalize path */
    char normalized[VFS_MAX_PATH];
    if (path_normalize(path, "/", normalized, sizeof(normalized)) < 0) {
        return -1;
    }

    /* Look up in RAMFS */
    uint32_t inode_num;
    if (ramfs_lookup_path(normalized, &inode_num) < 0) {
        return -1;
    }

    /* Get or create vnode */
    vnode_t* vn = vnode_get_or_create(inode_num);
    if (!vn) {
        return -1;
    }

    *result = vn;
    return 0;
}

/**
 * Look up the parent directory of a path.
 *
 * Returns parent vnode and copies the filename to 'name'.
 * Returns 0 on success, -1 on error.
 */
int vfs_lookup_parent(const char* path, vnode_t** parent, char* name, size_t name_size) {
    if (!path || !parent || !name || name_size == 0) {
        return -1;
    }

    /* Normalize path */
    char normalized[VFS_MAX_PATH];
    if (path_normalize(path, "/", normalized, sizeof(normalized)) < 0) {
        return -1;
    }

    /* Get dirname */
    char parent_path[VFS_MAX_PATH];
    if (path_dirname(normalized, parent_path, sizeof(parent_path)) < 0) {
        return -1;
    }

    /* Get basename */
    const char* basename = path_basename(normalized);
    size_t basename_len = strlen(basename);
    if (basename_len >= name_size) {
        return -1;
    }
    memcpy(name, basename, basename_len);
    name[basename_len] = '\0';

    /* Look up parent */
    return vfs_lookup(parent_path, parent);
}

/* =============================================================================
 * File Operations
 * =============================================================================
 */

/**
 * Open a file.
 *
 * Returns 0 on success, -1 on error.
 */
int vfs_open(const char* path, uint32_t flags, file_t** result) {
    if (!path || !result) {
        return -1;
    }

    vnode_t* vn = NULL;
    bool created = false;

    /* Try to look up the file */
    if (vfs_lookup(path, &vn) < 0) {
        /* File doesn't exist */
        if (flags & O_CREAT) {
            /* Create the file */
            vnode_t* parent;
            char name[RAMFS_MAX_FILENAME];
            if (vfs_lookup_parent(path, &parent, name, sizeof(name)) < 0) {
                return -1;
            }

            if (parent->ops && parent->ops->create) {
                if (parent->ops->create(parent, name, INODE_TYPE_FILE, &vn) < 0) {
                    vnode_unref(parent);
                    return -1;
                }
                created = true;
            }
            vnode_unref(parent);

            if (!vn) {
                return -1;
            }
        } else {
            return -1;  /* File not found and O_CREAT not set */
        }
    }

    /* Check type */
    if (vn->type == INODE_TYPE_DIR && (flags & O_ACCMODE) != O_RDONLY) {
        vnode_unref(vn);
        return -1;  /* Can't write to directory */
    }

    /* Handle O_TRUNC */
    if ((flags & O_TRUNC) && vn->type == INODE_TYPE_FILE && !created) {
        if (vn->ops && vn->ops->truncate) {
            vn->ops->truncate(vn, 0);
        }
    }

    /* Allocate file_t */
    file_t* file = file_alloc();
    if (!file) {
        vnode_unref(vn);
        return -1;
    }

    file->flags = flags;
    file->offset = (flags & O_APPEND) ? vn->inode->size : 0;
    file->inode = vn->inode_num;
    file->type = (vn->type == INODE_TYPE_DIR) ? FILE_TYPE_DIR : FILE_TYPE_REGULAR;
    file->vnode = vn;

    *result = file;
    return 0;
}

/**
 * Close a file.
 */
int vfs_close(file_t* file) {
    if (!file) {
        return -1;
    }

    /* file_unref handles cleanup */
    file_unref(file);
    return 0;
}

/**
 * Read from a file.
 *
 * Returns number of bytes read, or negative error code.
 */
int64_t vfs_read(file_t* file, void* buf, size_t count) {
    if (!file || !buf) {
        return -1;
    }

    if ((file->flags & O_ACCMODE) == O_WRONLY) {
        return -1;  /* Can't read write-only file */
    }

    if (!file->vnode || !file->vnode->ops || !file->vnode->ops->read) {
        return -1;
    }

    int64_t bytes = file->vnode->ops->read(file->vnode, buf, count, file->offset);
    if (bytes > 0) {
        file->offset += bytes;
    }

    return bytes;
}

/**
 * Write to a file.
 *
 * Returns number of bytes written, or negative error code.
 */
int64_t vfs_write(file_t* file, const void* buf, size_t count) {
    if (!file || !buf) {
        return -1;
    }

    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        return -1;  /* Can't write read-only file */
    }

    if (!file->vnode || !file->vnode->ops || !file->vnode->ops->write) {
        return -1;
    }

    /* Handle O_APPEND */
    if (file->flags & O_APPEND) {
        file->offset = file->vnode->inode->size;
    }

    int64_t bytes = file->vnode->ops->write(file->vnode, buf, count, file->offset);
    if (bytes > 0) {
        file->offset += bytes;
    }

    return bytes;
}

/**
 * Seek in a file.
 *
 * Returns new offset, or negative error code.
 */
int64_t vfs_lseek(file_t* file, int64_t offset, int whence) {
    if (!file) {
        return -1;
    }

    if (file->type == FILE_TYPE_CONSOLE) {
        return -1;  /* Can't seek console */
    }

    int64_t new_offset;

    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = (int64_t)file->offset + offset;
            break;
        case SEEK_END:
            if (!file->vnode || !file->vnode->inode) {
                return -1;
            }
            new_offset = (int64_t)file->vnode->inode->size + offset;
            break;
        default:
            return -1;
    }

    if (new_offset < 0) {
        return -1;  /* Invalid offset */
    }

    file->offset = (uint64_t)new_offset;
    return new_offset;
}

/* =============================================================================
 * Stat Operations
 * =============================================================================
 */

/**
 * Get file status by path.
 */
int vfs_stat(const char* path, stat_t* buf) {
    if (!path || !buf) {
        return -1;
    }

    vnode_t* vn;
    if (vfs_lookup(path, &vn) < 0) {
        return -1;
    }

    int ret = 0;
    if (vn->ops && vn->ops->stat) {
        ret = vn->ops->stat(vn, buf);
    } else {
        ret = -1;
    }

    vnode_unref(vn);
    return ret;
}

/**
 * Get file status by file descriptor.
 */
int vfs_fstat(file_t* file, stat_t* buf) {
    if (!file || !buf) {
        return -1;
    }

    if (!file->vnode) {
        return -1;
    }

    if (file->vnode->ops && file->vnode->ops->stat) {
        return file->vnode->ops->stat(file->vnode, buf);
    }

    return -1;
}

/* =============================================================================
 * Directory Operations
 * =============================================================================
 */

/**
 * Create a directory.
 */
int vfs_mkdir(const char* path) {
    if (!path) {
        return -1;
    }

    vnode_t* parent;
    char name[RAMFS_MAX_FILENAME];

    if (vfs_lookup_parent(path, &parent, name, sizeof(name)) < 0) {
        return -1;
    }

    int ret = -1;
    if (parent->ops && parent->ops->create) {
        vnode_t* new_vn;
        ret = parent->ops->create(parent, name, INODE_TYPE_DIR, &new_vn);
        if (ret == 0 && new_vn) {
            vnode_unref(new_vn);
        }
    }

    vnode_unref(parent);
    return ret;
}

/**
 * Read a directory entry.
 */
int vfs_readdir(file_t* dir, ramfs_dirent_t* entry, uint32_t index) {
    if (!dir || !entry) {
        return -1;
    }

    if (dir->type != FILE_TYPE_DIR) {
        return -1;
    }

    if (!dir->vnode || !dir->vnode->ops || !dir->vnode->ops->readdir) {
        return -1;
    }

    return dir->vnode->ops->readdir(dir->vnode, index, entry);
}

/* =============================================================================
 * File Creation/Deletion
 * =============================================================================
 */

/**
 * Create a file or directory.
 */
int vfs_create(const char* path, uint32_t type) {
    if (!path) {
        return -1;
    }

    vnode_t* parent;
    char name[RAMFS_MAX_FILENAME];

    if (vfs_lookup_parent(path, &parent, name, sizeof(name)) < 0) {
        return -1;
    }

    int ret = -1;
    if (parent->ops && parent->ops->create) {
        vnode_t* new_vn;
        ret = parent->ops->create(parent, name, type, &new_vn);
        if (ret == 0 && new_vn) {
            vnode_unref(new_vn);
        }
    }

    vnode_unref(parent);
    return ret;
}

/**
 * Remove a file.
 */
int vfs_unlink(const char* path) {
    if (!path) {
        return -1;
    }

    vnode_t* parent;
    char name[RAMFS_MAX_FILENAME];

    if (vfs_lookup_parent(path, &parent, name, sizeof(name)) < 0) {
        return -1;
    }

    int ret = -1;
    if (parent->ops && parent->ops->unlink) {
        ret = parent->ops->unlink(parent, name);
    }

    vnode_unref(parent);
    return ret;
}

/* =============================================================================
 * RAMFS VFS Operations Implementation
 * =============================================================================
 */

static int64_t ramfs_vfs_read(vnode_t* vn, void* buf, size_t count, uint64_t offset) {
    if (!vn || !vn->inode) {
        return -1;
    }
    return ramfs_read(vn->inode, buf, count, offset);
}

static int64_t ramfs_vfs_write(vnode_t* vn, const void* buf, size_t count, uint64_t offset) {
    if (!vn || !vn->inode) {
        return -1;
    }
    return ramfs_write(vn->inode, buf, count, offset);
}

static int ramfs_vfs_lookup(vnode_t* dir, const char* name, vnode_t** result) {
    if (!dir || !dir->inode || !name || !result) {
        return -1;
    }

    uint32_t inode_num;
    if (ramfs_dir_lookup(dir->inode, name, &inode_num) < 0) {
        return -1;
    }

    *result = vnode_get_or_create(inode_num);
    return *result ? 0 : -1;
}

static int ramfs_vfs_create(vnode_t* dir, const char* name, uint32_t type, vnode_t** result) {
    if (!dir || !dir->inode || !name || !result) {
        return -1;
    }

    /* Allocate new inode */
    uint32_t new_inode;
    if (ramfs_alloc_inode(type, &new_inode) < 0) {
        return -1;
    }

    /* Set parent */
    ramfs_inode_t* inode = ramfs_get_inode(new_inode);
    if (inode) {
        inode->parent = dir->inode_num;
        if (type == INODE_TYPE_DIR) {
            inode->link_count = 2;
            dir->inode->link_count++;
        }
    }

    /* Add to parent directory */
    if (ramfs_dir_add_entry(dir->inode, name, new_inode, type) < 0) {
        if (type == INODE_TYPE_DIR) {
            dir->inode->link_count--;
        }
        ramfs_free_inode(new_inode);
        return -1;
    }

    *result = vnode_get_or_create(new_inode);
    return *result ? 0 : -1;
}

static int ramfs_vfs_unlink(vnode_t* dir, const char* name) {
    if (!dir || !dir->inode || !name) {
        return -1;
    }

    /* Look up the entry */
    uint32_t inode_num;
    if (ramfs_dir_lookup(dir->inode, name, &inode_num) < 0) {
        return -1;
    }

    ramfs_inode_t* inode = ramfs_get_inode(inode_num);
    if (!inode) {
        return -1;
    }

    /* Don't allow unlinking directories with entries */
    if (inode->type == INODE_TYPE_DIR && inode->size > 0) {
        return -1;  /* Directory not empty */
    }

    /* Remove from parent */
    if (ramfs_dir_remove_entry(dir->inode, name) < 0) {
        return -1;
    }

    /* Update link counts */
    if (inode->type == INODE_TYPE_DIR) {
        dir->inode->link_count--;
    }
    inode->link_count--;

    /* Free inode if no more links */
    if (inode->link_count == 0) {
        ramfs_free_inode(inode_num);
    }

    return 0;
}

static int ramfs_vfs_readdir(vnode_t* dir, uint32_t index, ramfs_dirent_t* entry) {
    if (!dir || !dir->inode || !entry) {
        return -1;
    }
    return ramfs_dir_read_entry(dir->inode, index, entry);
}

static int ramfs_vfs_stat(vnode_t* vn, stat_t* buf) {
    if (!vn || !vn->inode || !buf) {
        return -1;
    }

    memset(buf, 0, sizeof(stat_t));

    buf->st_ino = vn->inode_num;
    buf->st_nlink = vn->inode->link_count;
    buf->st_uid = vn->inode->uid;
    buf->st_gid = vn->inode->gid;
    buf->st_size = vn->inode->size;
    buf->st_blksize = RAMFS_BLOCK_SIZE;
    buf->st_blocks = vn->inode->block_count;
    buf->st_atime = vn->inode->accessed;
    buf->st_mtime = vn->inode->modified;
    buf->st_ctime = vn->inode->created;

    /* Set mode (type + permissions) */
    buf->st_mode = vn->inode->permissions;
    if (vn->inode->type == INODE_TYPE_FILE) {
        buf->st_mode |= S_IFREG;
    } else if (vn->inode->type == INODE_TYPE_DIR) {
        buf->st_mode |= S_IFDIR;
    }

    return 0;
}

static int ramfs_vfs_truncate(vnode_t* vn, uint64_t size) {
    if (!vn || !vn->inode) {
        return -1;
    }
    return ramfs_truncate(vn->inode, size);
}
