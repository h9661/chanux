/*
 * ramfs.h - RAM-based filesystem structures
 *
 * Simple in-memory filesystem for Chanux OS.
 * Educational implementation focusing on clarity.
 */

#ifndef _KERNEL_FS_RAMFS_H
#define _KERNEL_FS_RAMFS_H

#include "../types.h"

/* RAMFS constants */
#define RAMFS_MAGIC         0x52414D46  /* "RAMF" in little-endian */
#define RAMFS_VERSION       1
#define RAMFS_MAX_FILES     256         /* Maximum number of files/dirs */
#define RAMFS_MAX_FILENAME  60          /* Maximum filename length */
#define RAMFS_BLOCK_SIZE    4096        /* Block size (matches page size) */
#define RAMFS_MAX_BLOCKS    1024        /* 4MB total filesystem size */
#define RAMFS_ROOT_INODE    0           /* Root directory inode number */

/* Inode types */
#define INODE_TYPE_FREE     0           /* Unused inode */
#define INODE_TYPE_FILE     1           /* Regular file */
#define INODE_TYPE_DIR      2           /* Directory */
#define INODE_TYPE_SPECIAL  3           /* Special file (device) */

/* Permission bits (POSIX-like) */
#define INODE_PERM_READ     0x0004
#define INODE_PERM_WRITE    0x0002
#define INODE_PERM_EXEC     0x0001

/* Default permissions */
#define INODE_PERM_DEFAULT_FILE  (INODE_PERM_READ | INODE_PERM_WRITE)
#define INODE_PERM_DEFAULT_DIR   (INODE_PERM_READ | INODE_PERM_WRITE | INODE_PERM_EXEC)

/* Number of direct block pointers per inode */
#define RAMFS_DIRECT_BLOCKS 12

/*
 * RAMFS Superblock
 *
 * Located at block 0 of the RAM disk.
 * Contains filesystem metadata and allocation bitmaps.
 */
typedef struct {
    uint32_t    magic;                              /* RAMFS_MAGIC */
    uint32_t    version;                            /* Filesystem version */
    uint32_t    block_size;                         /* Block size (4096) */
    uint32_t    total_blocks;                       /* Total blocks in filesystem */
    uint32_t    free_blocks;                        /* Free blocks available */
    uint32_t    total_inodes;                       /* Total inode slots */
    uint32_t    free_inodes;                        /* Free inode slots */
    uint32_t    root_inode;                         /* Root directory inode number */
    uint64_t    created_time;                       /* Creation timestamp (ticks) */
    uint64_t    mount_time;                         /* Last mount timestamp */
    uint8_t     block_bitmap[RAMFS_MAX_BLOCKS / 8]; /* Block allocation bitmap */
    uint8_t     inode_bitmap[RAMFS_MAX_FILES / 8];  /* Inode allocation bitmap */
    uint8_t     reserved[3896];                     /* Pad to 4096 bytes */
} PACKED ramfs_superblock_t;

/*
 * RAMFS Inode
 *
 * Represents a file or directory in the filesystem.
 * Size: 128 bytes (32 inodes per block)
 */
typedef struct {
    uint32_t    type;                           /* INODE_TYPE_* */
    uint32_t    permissions;                    /* Permission bits */
    uint32_t    uid;                            /* Owner user ID (future use) */
    uint32_t    gid;                            /* Owner group ID (future use) */
    uint64_t    size;                           /* File size in bytes */
    uint64_t    created;                        /* Creation time (ticks) */
    uint64_t    modified;                       /* Last modification time */
    uint64_t    accessed;                       /* Last access time */
    uint32_t    link_count;                     /* Number of hard links */
    uint32_t    block_count;                    /* Number of allocated blocks */
    uint32_t    blocks[RAMFS_DIRECT_BLOCKS];    /* Direct block pointers */
    uint32_t    indirect;                       /* Single indirect block (future) */
    uint32_t    parent;                         /* Parent directory inode number */
    uint8_t     reserved[4];                    /* Pad to 128 bytes */
} PACKED ramfs_inode_t;

/*
 * RAMFS Directory Entry
 *
 * Used to map names to inodes within a directory.
 * Size: 64 bytes (64 entries per block)
 */
typedef struct {
    uint32_t    inode;                          /* Inode number */
    uint16_t    rec_len;                        /* Record length (for future use) */
    uint8_t     name_len;                       /* Actual filename length */
    uint8_t     type;                           /* INODE_TYPE_* for quick access */
    char        name[RAMFS_MAX_FILENAME - 4];   /* Filename (null-terminated) */
} PACKED ramfs_dirent_t;

/*
 * RAMFS Memory Layout
 *
 * Block 0:       Superblock
 * Blocks 1-8:    Inode table (256 inodes * 128 bytes = 8 blocks)
 * Blocks 9+:     Data blocks
 */
#define RAMFS_SUPERBLOCK_BLOCK  0
#define RAMFS_INODE_START_BLOCK 1
#define RAMFS_INODE_BLOCKS      8   /* Holds 256 inodes */
#define RAMFS_DATA_START_BLOCK  9

/* Inodes per block */
#define RAMFS_INODES_PER_BLOCK  (RAMFS_BLOCK_SIZE / sizeof(ramfs_inode_t))

/* Directory entries per block */
#define RAMFS_DIRENTS_PER_BLOCK (RAMFS_BLOCK_SIZE / sizeof(ramfs_dirent_t))

/*
 * RAM Disk structure
 *
 * Represents the underlying block device (memory region).
 */
typedef struct {
    void*       data;           /* RAM disk memory region */
    size_t      size;           /* Total size in bytes */
    uint32_t    block_count;    /* Number of blocks */
    uint32_t    block_size;     /* Block size */
    bool        initialized;    /* Is device ready? */
} ramdisk_t;

/* Global RAMFS state (defined in ramfs.c) */
extern ramdisk_t g_ramdisk;
extern ramfs_superblock_t* g_superblock;

/* RAM disk initialization */
int ramdisk_init(size_t size_bytes);
int ramdisk_read_block(uint32_t block_num, void* buffer);
int ramdisk_write_block(uint32_t block_num, const void* buffer);
void* ramdisk_get_block_ptr(uint32_t block_num);

/* RAMFS initialization and formatting */
int ramfs_init(void);
int ramfs_format(void);

/* Inode operations */
int ramfs_alloc_inode(uint32_t type, uint32_t* inode_num);
void ramfs_free_inode(uint32_t inode_num);
ramfs_inode_t* ramfs_get_inode(uint32_t inode_num);

/* Block operations */
int ramfs_alloc_block(uint32_t* block_num);
void ramfs_free_block(uint32_t block_num);
void* ramfs_get_block(uint32_t block_num);

/* File content operations */
int64_t ramfs_read(ramfs_inode_t* inode, void* buf, size_t count, uint64_t offset);
int64_t ramfs_write(ramfs_inode_t* inode, const void* buf, size_t count, uint64_t offset);
int ramfs_truncate(ramfs_inode_t* inode, uint64_t new_size);

/* Directory operations */
int ramfs_dir_lookup(ramfs_inode_t* dir, const char* name, uint32_t* inode_out);
int ramfs_dir_add_entry(ramfs_inode_t* dir, const char* name, uint32_t inode, uint32_t type);
int ramfs_dir_remove_entry(ramfs_inode_t* dir, const char* name);
int ramfs_dir_read_entry(ramfs_inode_t* dir, uint32_t index, ramfs_dirent_t* entry);

/* High-level file operations */
int ramfs_create_file(const char* path, uint32_t* inode_out);
int ramfs_create_dir(const char* path, uint32_t* inode_out);
int ramfs_lookup_path(const char* path, uint32_t* inode_out);

#endif /* _KERNEL_FS_RAMFS_H */
