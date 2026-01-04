/*
 * vfs.h - Virtual Filesystem layer
 *
 * Provides a unified interface for filesystem operations.
 * Currently supports RAMFS, designed for future extension.
 */

#ifndef _KERNEL_FS_VFS_H
#define _KERNEL_FS_VFS_H

#include "../types.h"
#include "ramfs.h"
#include "file.h"

/* Maximum path length */
#define VFS_MAX_PATH    256

/* Stat structure (POSIX-like) */
typedef struct stat {
    uint32_t    st_ino;         /* Inode number */
    uint32_t    st_mode;        /* File type and permissions */
    uint32_t    st_nlink;       /* Number of hard links */
    uint32_t    st_uid;         /* Owner user ID */
    uint32_t    st_gid;         /* Owner group ID */
    uint64_t    st_size;        /* File size in bytes */
    uint64_t    st_blksize;     /* Block size for I/O */
    uint64_t    st_blocks;      /* Number of blocks allocated */
    uint64_t    st_atime;       /* Access time */
    uint64_t    st_mtime;       /* Modification time */
    uint64_t    st_ctime;       /* Creation time */
} stat_t;

/* Mode flags for st_mode (matches inode types) */
#define S_IFREG     0x8000      /* Regular file */
#define S_IFDIR     0x4000      /* Directory */
#define S_IFCHR     0x2000      /* Character device */

/* Forward declaration */
struct vfs_ops;

/*
 * VFS Node (vnode)
 *
 * Represents an open file or directory in the VFS layer.
 * Provides abstraction over the underlying filesystem.
 */
typedef struct vnode {
    uint32_t            inode_num;      /* Inode number */
    uint32_t            type;           /* INODE_TYPE_* */
    uint32_t            ref_count;      /* Reference count */
    ramfs_inode_t*      inode;          /* Pointer to underlying inode */
    struct vfs_ops*     ops;            /* Filesystem operations */
    void*               fs_data;        /* Filesystem-specific data */
} vnode_t;

/*
 * VFS Operations
 *
 * Function pointers for filesystem-specific operations.
 * Allows different filesystems to be plugged in.
 */
typedef struct vfs_ops {
    int64_t (*read)(vnode_t* vn, void* buf, size_t count, uint64_t offset);
    int64_t (*write)(vnode_t* vn, const void* buf, size_t count, uint64_t offset);
    int (*lookup)(vnode_t* dir, const char* name, vnode_t** result);
    int (*create)(vnode_t* dir, const char* name, uint32_t type, vnode_t** result);
    int (*unlink)(vnode_t* dir, const char* name);
    int (*readdir)(vnode_t* dir, uint32_t index, ramfs_dirent_t* entry);
    int (*stat)(vnode_t* vn, stat_t* buf);
    int (*truncate)(vnode_t* vn, uint64_t size);
} vfs_ops_t;

/* Global root vnode (defined in vfs.c) */
extern vnode_t* g_root_vnode;

/* VFS initialization */
void vfs_init(void);

/* Vnode management */
vnode_t* vnode_alloc(void);
void vnode_free(vnode_t* vn);
void vnode_ref(vnode_t* vn);
void vnode_unref(vnode_t* vn);

/* Path operations */
int vfs_lookup(const char* path, vnode_t** result);
int vfs_lookup_parent(const char* path, vnode_t** parent, char* name, size_t name_size);

/* File operations */
int vfs_open(const char* path, uint32_t flags, file_t** result);
int vfs_close(file_t* file);
int64_t vfs_read(file_t* file, void* buf, size_t count);
int64_t vfs_write(file_t* file, const void* buf, size_t count);
int64_t vfs_lseek(file_t* file, int64_t offset, int whence);

/* Stat operations */
int vfs_stat(const char* path, stat_t* buf);
int vfs_fstat(file_t* file, stat_t* buf);

/* Directory operations */
int vfs_mkdir(const char* path);
int vfs_rmdir(const char* path);
int vfs_readdir(file_t* dir, ramfs_dirent_t* entry, uint32_t index);

/* File creation/deletion */
int vfs_create(const char* path, uint32_t type);
int vfs_unlink(const char* path);

/* Path utilities (defined in path.c) */
int path_is_absolute(const char* path);
const char* path_basename(const char* path);
int path_dirname(const char* path, char* buf, size_t size);
int path_normalize(const char* path, const char* cwd, char* buf, size_t size);
int path_join(const char* dir, const char* name, char* buf, size_t size);

#endif /* _KERNEL_FS_VFS_H */
