/**
 * =============================================================================
 * Chanux OS - RAM Filesystem Implementation
 * =============================================================================
 * Simple in-memory filesystem for educational purposes.
 *
 * Memory Layout:
 *   Block 0:       Superblock
 *   Blocks 1-8:    Inode table (256 inodes * 128 bytes = 8 blocks)
 *   Blocks 9+:     Data blocks
 *
 * Design:
 *   - All data is stored in RAM (volatile)
 *   - Simple bitmap-based allocation for blocks and inodes
 *   - Direct block pointers only (no indirect blocks for simplicity)
 *   - Maximum file size: 12 * 4096 = 48KB
 * =============================================================================
 */

#include "fs/ramfs.h"
#include "mm/heap.h"
#include "kernel.h"
#include "string.h"
#include "drivers/vga/vga.h"
#include "drivers/pit.h"

/* Maximum path length (same as RAMFS_MAX_PATH in vfs.h) */
#ifndef RAMFS_MAX_PATH
#define RAMFS_MAX_PATH 256
#endif

/* Global RAMFS state */
ramdisk_t g_ramdisk = {0};
ramfs_superblock_t* g_superblock = NULL;

/* Debug flag */
#define DEBUG_RAMFS 0

#if DEBUG_RAMFS
#define RAMFS_DEBUG(fmt, ...) kprintf("[RAMFS] " fmt, ##__VA_ARGS__)
#else
#define RAMFS_DEBUG(fmt, ...) ((void)0)
#endif

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * Set a bit in a bitmap.
 */
static void bitmap_set(uint8_t* bitmap, uint32_t index) {
    bitmap[index / 8] |= (1 << (index % 8));
}

/**
 * Clear a bit in a bitmap.
 */
static void bitmap_clear(uint8_t* bitmap, uint32_t index) {
    bitmap[index / 8] &= ~(1 << (index % 8));
}

/**
 * Test a bit in a bitmap.
 */
static bool bitmap_test(uint8_t* bitmap, uint32_t index) {
    return (bitmap[index / 8] & (1 << (index % 8))) != 0;
}

/**
 * Find first free bit in a bitmap.
 * Returns -1 if no free bit found.
 */
static int bitmap_find_free(uint8_t* bitmap, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        if (!bitmap_test(bitmap, i)) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * Minimum of two values.
 */
static size_t min_size(size_t a, size_t b) {
    return (a < b) ? a : b;
}

/* =============================================================================
 * RAM Disk Layer
 * =============================================================================
 */

/**
 * Initialize the RAM disk.
 *
 * Allocates memory for the filesystem.
 * Returns 0 on success, -1 on failure.
 */
int ramdisk_init(size_t size_bytes) {
    if (g_ramdisk.initialized) {
        return 0;  /* Already initialized */
    }

    /* Ensure size is a multiple of block size */
    size_bytes = (size_bytes + RAMFS_BLOCK_SIZE - 1) & ~(RAMFS_BLOCK_SIZE - 1);

    /* Allocate RAM disk memory */
    g_ramdisk.data = kmalloc(size_bytes);
    if (!g_ramdisk.data) {
        kprintf("[RAMFS] Error: Failed to allocate %lu bytes for RAM disk\n",
                (unsigned long)size_bytes);
        return -1;
    }

    /* Clear the RAM disk */
    memset(g_ramdisk.data, 0, size_bytes);

    g_ramdisk.size = size_bytes;
    g_ramdisk.block_count = size_bytes / RAMFS_BLOCK_SIZE;
    g_ramdisk.block_size = RAMFS_BLOCK_SIZE;
    g_ramdisk.initialized = true;

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[RAMFS] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("RAM disk initialized: %lu KB (%u blocks)\n",
            (unsigned long)(size_bytes / 1024), g_ramdisk.block_count);

    return 0;
}

/**
 * Read a block from the RAM disk.
 */
int ramdisk_read_block(uint32_t block_num, void* buffer) {
    if (!g_ramdisk.initialized || block_num >= g_ramdisk.block_count) {
        return -1;
    }

    uint8_t* src = (uint8_t*)g_ramdisk.data + (block_num * RAMFS_BLOCK_SIZE);
    memcpy(buffer, src, RAMFS_BLOCK_SIZE);
    return 0;
}

/**
 * Write a block to the RAM disk.
 */
int ramdisk_write_block(uint32_t block_num, const void* buffer) {
    if (!g_ramdisk.initialized || block_num >= g_ramdisk.block_count) {
        return -1;
    }

    uint8_t* dst = (uint8_t*)g_ramdisk.data + (block_num * RAMFS_BLOCK_SIZE);
    memcpy(dst, buffer, RAMFS_BLOCK_SIZE);
    return 0;
}

/**
 * Get direct pointer to a block in the RAM disk.
 * This avoids copying for in-memory filesystem.
 */
void* ramdisk_get_block_ptr(uint32_t block_num) {
    if (!g_ramdisk.initialized || block_num >= g_ramdisk.block_count) {
        return NULL;
    }

    return (uint8_t*)g_ramdisk.data + (block_num * RAMFS_BLOCK_SIZE);
}

/* =============================================================================
 * RAMFS Initialization
 * =============================================================================
 */

/**
 * Format the filesystem.
 * Creates superblock and root directory.
 */
int ramfs_format(void) {
    if (!g_ramdisk.initialized) {
        return -1;
    }

    /* Initialize superblock */
    g_superblock = (ramfs_superblock_t*)ramdisk_get_block_ptr(RAMFS_SUPERBLOCK_BLOCK);
    if (!g_superblock) {
        return -1;
    }

    memset(g_superblock, 0, sizeof(ramfs_superblock_t));

    g_superblock->magic = RAMFS_MAGIC;
    g_superblock->version = RAMFS_VERSION;
    g_superblock->block_size = RAMFS_BLOCK_SIZE;
    g_superblock->total_blocks = g_ramdisk.block_count;
    g_superblock->free_blocks = g_ramdisk.block_count - RAMFS_DATA_START_BLOCK;
    g_superblock->total_inodes = RAMFS_MAX_FILES;
    g_superblock->free_inodes = RAMFS_MAX_FILES - 1;  /* Root inode is used */
    g_superblock->root_inode = RAMFS_ROOT_INODE;
    g_superblock->created_time = pit_get_ticks();
    g_superblock->mount_time = g_superblock->created_time;

    /* Mark superblock and inode table blocks as used */
    for (uint32_t i = 0; i < RAMFS_DATA_START_BLOCK; i++) {
        bitmap_set(g_superblock->block_bitmap, i);
    }

    /* Create root directory inode */
    ramfs_inode_t* root = ramfs_get_inode(RAMFS_ROOT_INODE);
    if (!root) {
        return -1;
    }

    memset(root, 0, sizeof(ramfs_inode_t));
    root->type = INODE_TYPE_DIR;
    root->permissions = INODE_PERM_DEFAULT_DIR;
    root->size = 0;
    root->created = pit_get_ticks();
    root->modified = root->created;
    root->accessed = root->created;
    root->link_count = 2;  /* . and parent (for root, parent is self) */
    root->parent = RAMFS_ROOT_INODE;  /* Root's parent is itself */

    /* Mark root inode as used */
    bitmap_set(g_superblock->inode_bitmap, RAMFS_ROOT_INODE);

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[RAMFS] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Filesystem formatted: %u blocks, %u inodes\n",
            g_superblock->total_blocks, g_superblock->total_inodes);

    return 0;
}

/**
 * Initialize RAMFS.
 * Creates RAM disk and formats the filesystem.
 */
int ramfs_init(void) {
    /* Initialize RAM disk (4MB default) */
    if (ramdisk_init(RAMFS_MAX_BLOCKS * RAMFS_BLOCK_SIZE) < 0) {
        return -1;
    }

    /* Format the filesystem */
    if (ramfs_format() < 0) {
        return -1;
    }

    return 0;
}

/* =============================================================================
 * Inode Operations
 * =============================================================================
 */

/**
 * Get pointer to an inode by number.
 */
ramfs_inode_t* ramfs_get_inode(uint32_t inode_num) {
    if (inode_num >= RAMFS_MAX_FILES) {
        return NULL;
    }

    /* Calculate which block and offset within block */
    uint32_t inodes_per_block = RAMFS_BLOCK_SIZE / sizeof(ramfs_inode_t);
    uint32_t block_num = RAMFS_INODE_START_BLOCK + (inode_num / inodes_per_block);
    uint32_t offset = inode_num % inodes_per_block;

    ramfs_inode_t* block = (ramfs_inode_t*)ramdisk_get_block_ptr(block_num);
    if (!block) {
        return NULL;
    }

    return &block[offset];
}

/**
 * Allocate a new inode.
 * Returns 0 on success, -1 on failure.
 */
int ramfs_alloc_inode(uint32_t type, uint32_t* inode_num) {
    if (!g_superblock || !inode_num) {
        return -1;
    }

    if (g_superblock->free_inodes == 0) {
        return -1;  /* No free inodes */
    }

    /* Find free inode */
    int free_inode = bitmap_find_free(g_superblock->inode_bitmap, RAMFS_MAX_FILES);
    if (free_inode < 0) {
        return -1;
    }

    /* Mark inode as used */
    bitmap_set(g_superblock->inode_bitmap, free_inode);
    g_superblock->free_inodes--;

    /* Initialize the inode */
    ramfs_inode_t* inode = ramfs_get_inode(free_inode);
    if (!inode) {
        /* Rollback */
        bitmap_clear(g_superblock->inode_bitmap, free_inode);
        g_superblock->free_inodes++;
        return -1;
    }

    memset(inode, 0, sizeof(ramfs_inode_t));
    inode->type = type;
    inode->permissions = (type == INODE_TYPE_DIR) ?
                         INODE_PERM_DEFAULT_DIR : INODE_PERM_DEFAULT_FILE;
    inode->created = pit_get_ticks();
    inode->modified = inode->created;
    inode->accessed = inode->created;
    inode->link_count = 1;

    *inode_num = (uint32_t)free_inode;
    RAMFS_DEBUG("Allocated inode %u (type %u)\n", *inode_num, type);

    return 0;
}

/**
 * Free an inode.
 */
void ramfs_free_inode(uint32_t inode_num) {
    if (!g_superblock || inode_num >= RAMFS_MAX_FILES) {
        return;
    }

    if (inode_num == RAMFS_ROOT_INODE) {
        return;  /* Never free root inode */
    }

    ramfs_inode_t* inode = ramfs_get_inode(inode_num);
    if (!inode || inode->type == INODE_TYPE_FREE) {
        return;
    }

    /* Free all data blocks */
    for (int i = 0; i < RAMFS_DIRECT_BLOCKS; i++) {
        if (inode->blocks[i] != 0) {
            ramfs_free_block(inode->blocks[i]);
        }
    }

    /* Clear the inode */
    memset(inode, 0, sizeof(ramfs_inode_t));

    /* Mark inode as free */
    bitmap_clear(g_superblock->inode_bitmap, inode_num);
    g_superblock->free_inodes++;

    RAMFS_DEBUG("Freed inode %u\n", inode_num);
}

/* =============================================================================
 * Block Operations
 * =============================================================================
 */

/**
 * Allocate a data block.
 * Returns 0 on success, -1 on failure.
 */
int ramfs_alloc_block(uint32_t* block_num) {
    if (!g_superblock || !block_num) {
        return -1;
    }

    if (g_superblock->free_blocks == 0) {
        return -1;  /* No free blocks */
    }

    /* Find free block (start from data area) */
    for (uint32_t i = RAMFS_DATA_START_BLOCK; i < g_ramdisk.block_count; i++) {
        if (!bitmap_test(g_superblock->block_bitmap, i)) {
            /* Mark block as used */
            bitmap_set(g_superblock->block_bitmap, i);
            g_superblock->free_blocks--;

            /* Clear the block */
            void* block_ptr = ramdisk_get_block_ptr(i);
            if (block_ptr) {
                memset(block_ptr, 0, RAMFS_BLOCK_SIZE);
            }

            *block_num = i;
            RAMFS_DEBUG("Allocated block %u\n", i);
            return 0;
        }
    }

    return -1;  /* No free blocks found */
}

/**
 * Free a data block.
 */
void ramfs_free_block(uint32_t block_num) {
    if (!g_superblock || block_num < RAMFS_DATA_START_BLOCK ||
        block_num >= g_ramdisk.block_count) {
        return;
    }

    if (!bitmap_test(g_superblock->block_bitmap, block_num)) {
        return;  /* Already free */
    }

    bitmap_clear(g_superblock->block_bitmap, block_num);
    g_superblock->free_blocks++;

    RAMFS_DEBUG("Freed block %u\n", block_num);
}

/**
 * Get pointer to a data block.
 */
void* ramfs_get_block(uint32_t block_num) {
    return ramdisk_get_block_ptr(block_num);
}

/* =============================================================================
 * File Content Operations
 * =============================================================================
 */

/**
 * Read from a file.
 *
 * Returns number of bytes read, or negative error code.
 */
int64_t ramfs_read(ramfs_inode_t* inode, void* buf, size_t count, uint64_t offset) {
    if (!inode || !buf) {
        return -1;
    }

    if (inode->type != INODE_TYPE_FILE) {
        return -1;  /* Can only read files */
    }

    /* Check bounds */
    if (offset >= inode->size) {
        return 0;  /* EOF */
    }

    /* Limit read to available data */
    if (offset + count > inode->size) {
        count = inode->size - offset;
    }

    uint8_t* dest = (uint8_t*)buf;
    size_t bytes_read = 0;

    while (bytes_read < count) {
        /* Calculate which block and offset within block */
        uint32_t block_index = (offset + bytes_read) / RAMFS_BLOCK_SIZE;
        uint32_t block_offset = (offset + bytes_read) % RAMFS_BLOCK_SIZE;

        if (block_index >= RAMFS_DIRECT_BLOCKS) {
            break;  /* Beyond direct blocks */
        }

        uint32_t block_num = inode->blocks[block_index];
        if (block_num == 0) {
            /* Sparse file - return zeros */
            size_t to_read = min_size(count - bytes_read, RAMFS_BLOCK_SIZE - block_offset);
            memset(dest + bytes_read, 0, to_read);
            bytes_read += to_read;
        } else {
            /* Read from block */
            void* block_ptr = ramfs_get_block(block_num);
            if (!block_ptr) {
                break;
            }

            size_t to_read = min_size(count - bytes_read, RAMFS_BLOCK_SIZE - block_offset);
            memcpy(dest + bytes_read, (uint8_t*)block_ptr + block_offset, to_read);
            bytes_read += to_read;
        }
    }

    /* Update access time */
    inode->accessed = pit_get_ticks();

    return (int64_t)bytes_read;
}

/**
 * Write to a file.
 *
 * Returns number of bytes written, or negative error code.
 */
int64_t ramfs_write(ramfs_inode_t* inode, const void* buf, size_t count, uint64_t offset) {
    if (!inode || !buf) {
        return -1;
    }

    if (inode->type != INODE_TYPE_FILE) {
        return -1;  /* Can only write files */
    }

    /* Check if write would exceed max file size */
    uint64_t max_size = RAMFS_DIRECT_BLOCKS * RAMFS_BLOCK_SIZE;
    if (offset > max_size) {
        return -1;  /* Offset beyond max file size */
    }

    if (offset + count > max_size) {
        count = max_size - offset;  /* Truncate write */
    }

    const uint8_t* src = (const uint8_t*)buf;
    size_t bytes_written = 0;

    while (bytes_written < count) {
        /* Calculate which block and offset within block */
        uint32_t block_index = (offset + bytes_written) / RAMFS_BLOCK_SIZE;
        uint32_t block_offset = (offset + bytes_written) % RAMFS_BLOCK_SIZE;

        if (block_index >= RAMFS_DIRECT_BLOCKS) {
            break;  /* Beyond direct blocks */
        }

        /* Allocate block if needed */
        if (inode->blocks[block_index] == 0) {
            uint32_t new_block;
            if (ramfs_alloc_block(&new_block) < 0) {
                break;  /* No more blocks */
            }
            inode->blocks[block_index] = new_block;
            inode->block_count++;
        }

        /* Write to block */
        void* block_ptr = ramfs_get_block(inode->blocks[block_index]);
        if (!block_ptr) {
            break;
        }

        size_t to_write = min_size(count - bytes_written, RAMFS_BLOCK_SIZE - block_offset);
        memcpy((uint8_t*)block_ptr + block_offset, src + bytes_written, to_write);
        bytes_written += to_write;
    }

    /* Update file size if necessary */
    if (offset + bytes_written > inode->size) {
        inode->size = offset + bytes_written;
    }

    /* Update modification time */
    inode->modified = pit_get_ticks();
    inode->accessed = inode->modified;

    return (int64_t)bytes_written;
}

/**
 * Truncate a file to a new size.
 */
int ramfs_truncate(ramfs_inode_t* inode, uint64_t new_size) {
    if (!inode || inode->type != INODE_TYPE_FILE) {
        return -1;
    }

    uint64_t max_size = RAMFS_DIRECT_BLOCKS * RAMFS_BLOCK_SIZE;
    if (new_size > max_size) {
        new_size = max_size;
    }

    if (new_size < inode->size) {
        /* Shrinking - free unused blocks */
        uint32_t first_free_block = (new_size + RAMFS_BLOCK_SIZE - 1) / RAMFS_BLOCK_SIZE;
        for (uint32_t i = first_free_block; i < RAMFS_DIRECT_BLOCKS; i++) {
            if (inode->blocks[i] != 0) {
                ramfs_free_block(inode->blocks[i]);
                inode->blocks[i] = 0;
                inode->block_count--;
            }
        }
    }
    /* Note: Growing doesn't allocate blocks until write */

    inode->size = new_size;
    inode->modified = pit_get_ticks();

    return 0;
}

/* =============================================================================
 * Directory Operations
 * =============================================================================
 */

/**
 * Lookup a name in a directory.
 *
 * Returns 0 on success (inode_out set), -1 if not found.
 */
int ramfs_dir_lookup(ramfs_inode_t* dir, const char* name, uint32_t* inode_out) {
    if (!dir || !name || !inode_out) {
        return -1;
    }

    if (dir->type != INODE_TYPE_DIR) {
        return -1;
    }

    /* Handle special names */
    if (name[0] == '.' && name[1] == '\0') {
        /* Current directory - need to find inode number */
        /* For now, just return error - caller should handle this */
        return -1;
    }

    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        *inode_out = dir->parent;
        return 0;
    }

    /* Search directory entries */
    uint32_t entries_per_block = RAMFS_BLOCK_SIZE / sizeof(ramfs_dirent_t);
    size_t name_len = strlen(name);

    for (uint32_t i = 0; i < RAMFS_DIRECT_BLOCKS; i++) {
        if (dir->blocks[i] == 0) {
            continue;
        }

        ramfs_dirent_t* entries = (ramfs_dirent_t*)ramfs_get_block(dir->blocks[i]);
        if (!entries) {
            continue;
        }

        for (uint32_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode != 0 && entries[j].name_len == name_len) {
                if (strncmp(entries[j].name, name, name_len) == 0) {
                    *inode_out = entries[j].inode;
                    return 0;
                }
            }
        }
    }

    return -1;  /* Not found */
}

/**
 * Add an entry to a directory.
 *
 * Returns 0 on success, -1 on failure.
 */
int ramfs_dir_add_entry(ramfs_inode_t* dir, const char* name, uint32_t inode, uint32_t type) {
    if (!dir || !name || dir->type != INODE_TYPE_DIR) {
        return -1;
    }

    size_t name_len = strlen(name);
    if (name_len == 0 || name_len >= RAMFS_MAX_FILENAME) {
        return -1;
    }

    /* Check if name already exists */
    uint32_t existing;
    if (ramfs_dir_lookup(dir, name, &existing) == 0) {
        return -1;  /* Already exists */
    }

    /* Find a free entry slot */
    uint32_t entries_per_block = RAMFS_BLOCK_SIZE / sizeof(ramfs_dirent_t);

    for (uint32_t i = 0; i < RAMFS_DIRECT_BLOCKS; i++) {
        /* Allocate block if needed */
        if (dir->blocks[i] == 0) {
            uint32_t new_block;
            if (ramfs_alloc_block(&new_block) < 0) {
                return -1;
            }
            dir->blocks[i] = new_block;
            dir->block_count++;
        }

        ramfs_dirent_t* entries = (ramfs_dirent_t*)ramfs_get_block(dir->blocks[i]);
        if (!entries) {
            continue;
        }

        for (uint32_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode == 0) {
                /* Found free slot */
                entries[j].inode = inode;
                entries[j].rec_len = sizeof(ramfs_dirent_t);
                entries[j].name_len = (uint8_t)name_len;
                entries[j].type = (uint8_t)type;
                memcpy(entries[j].name, name, name_len);
                entries[j].name[name_len] = '\0';

                dir->size += sizeof(ramfs_dirent_t);
                dir->modified = pit_get_ticks();

                RAMFS_DEBUG("Added entry '%s' -> inode %u\n", name, inode);
                return 0;
            }
        }
    }

    return -1;  /* No free entry slots */
}

/**
 * Remove an entry from a directory.
 *
 * Returns 0 on success, -1 on failure.
 */
int ramfs_dir_remove_entry(ramfs_inode_t* dir, const char* name) {
    if (!dir || !name || dir->type != INODE_TYPE_DIR) {
        return -1;
    }

    size_t name_len = strlen(name);
    uint32_t entries_per_block = RAMFS_BLOCK_SIZE / sizeof(ramfs_dirent_t);

    for (uint32_t i = 0; i < RAMFS_DIRECT_BLOCKS; i++) {
        if (dir->blocks[i] == 0) {
            continue;
        }

        ramfs_dirent_t* entries = (ramfs_dirent_t*)ramfs_get_block(dir->blocks[i]);
        if (!entries) {
            continue;
        }

        for (uint32_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode != 0 && entries[j].name_len == name_len) {
                if (strncmp(entries[j].name, name, name_len) == 0) {
                    /* Clear the entry */
                    memset(&entries[j], 0, sizeof(ramfs_dirent_t));
                    dir->size -= sizeof(ramfs_dirent_t);
                    dir->modified = pit_get_ticks();

                    RAMFS_DEBUG("Removed entry '%s'\n", name);
                    return 0;
                }
            }
        }
    }

    return -1;  /* Not found */
}

/**
 * Read a directory entry by index.
 *
 * Returns 0 on success, -1 if index out of range or error.
 */
int ramfs_dir_read_entry(ramfs_inode_t* dir, uint32_t index, ramfs_dirent_t* entry) {
    if (!dir || !entry || dir->type != INODE_TYPE_DIR) {
        return -1;
    }

    uint32_t entries_per_block = RAMFS_BLOCK_SIZE / sizeof(ramfs_dirent_t);
    uint32_t current_index = 0;

    for (uint32_t i = 0; i < RAMFS_DIRECT_BLOCKS; i++) {
        if (dir->blocks[i] == 0) {
            continue;
        }

        ramfs_dirent_t* entries = (ramfs_dirent_t*)ramfs_get_block(dir->blocks[i]);
        if (!entries) {
            continue;
        }

        for (uint32_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode != 0) {
                if (current_index == index) {
                    memcpy(entry, &entries[j], sizeof(ramfs_dirent_t));
                    return 0;
                }
                current_index++;
            }
        }
    }

    return -1;  /* Index out of range */
}

/* =============================================================================
 * High-level Path Operations
 * =============================================================================
 */

/**
 * Look up a path and return the inode number.
 *
 * Supports absolute paths only (starting with /).
 * Returns 0 on success, -1 on failure.
 */
int ramfs_lookup_path(const char* path, uint32_t* inode_out) {
    if (!path || !inode_out) {
        return -1;
    }

    /* Must be absolute path */
    if (path[0] != '/') {
        return -1;
    }

    /* Root directory */
    if (path[0] == '/' && path[1] == '\0') {
        *inode_out = RAMFS_ROOT_INODE;
        return 0;
    }

    /* Traverse path components */
    uint32_t current_inode = RAMFS_ROOT_INODE;
    char component[RAMFS_MAX_FILENAME];
    const char* p = path + 1;  /* Skip leading / */

    while (*p) {
        /* Extract next component */
        size_t len = 0;
        while (p[len] && p[len] != '/') {
            if (len >= RAMFS_MAX_FILENAME - 1) {
                return -1;  /* Component too long */
            }
            component[len] = p[len];
            len++;
        }
        component[len] = '\0';

        if (len == 0) {
            /* Skip empty components (consecutive slashes) */
            p++;
            continue;
        }

        /* Look up component in current directory */
        ramfs_inode_t* dir = ramfs_get_inode(current_inode);
        if (!dir || dir->type != INODE_TYPE_DIR) {
            return -1;
        }

        uint32_t next_inode;
        if (ramfs_dir_lookup(dir, component, &next_inode) < 0) {
            return -1;  /* Not found */
        }

        current_inode = next_inode;
        p += len;

        /* Skip trailing slash */
        if (*p == '/') {
            p++;
        }
    }

    *inode_out = current_inode;
    return 0;
}

/**
 * Create a file at the given path.
 *
 * Parent directory must exist.
 * Returns 0 on success, -1 on failure.
 */
int ramfs_create_file(const char* path, uint32_t* inode_out) {
    if (!path || !inode_out || path[0] != '/') {
        return -1;
    }

    /* Find parent directory and filename */
    const char* last_slash = path;
    for (const char* p = path; *p; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }

    char parent_path[RAMFS_MAX_PATH];
    char filename[RAMFS_MAX_FILENAME];

    if (last_slash == path) {
        /* File in root directory */
        parent_path[0] = '/';
        parent_path[1] = '\0';
        strncpy(filename, path + 1, RAMFS_MAX_FILENAME - 1);
        filename[RAMFS_MAX_FILENAME - 1] = '\0';
    } else {
        /* Copy parent path */
        size_t parent_len = last_slash - path;
        if (parent_len >= RAMFS_MAX_PATH) {
            return -1;
        }
        memcpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';

        /* Copy filename */
        strncpy(filename, last_slash + 1, RAMFS_MAX_FILENAME - 1);
        filename[RAMFS_MAX_FILENAME - 1] = '\0';
    }

    if (filename[0] == '\0') {
        return -1;  /* Empty filename */
    }

    /* Look up parent directory */
    uint32_t parent_inode;
    if (ramfs_lookup_path(parent_path, &parent_inode) < 0) {
        return -1;  /* Parent doesn't exist */
    }

    ramfs_inode_t* parent = ramfs_get_inode(parent_inode);
    if (!parent || parent->type != INODE_TYPE_DIR) {
        return -1;
    }

    /* Check if file already exists */
    uint32_t existing;
    if (ramfs_dir_lookup(parent, filename, &existing) == 0) {
        return -1;  /* Already exists */
    }

    /* Allocate new inode */
    uint32_t new_inode;
    if (ramfs_alloc_inode(INODE_TYPE_FILE, &new_inode) < 0) {
        return -1;
    }

    /* Set parent */
    ramfs_inode_t* inode = ramfs_get_inode(new_inode);
    if (inode) {
        inode->parent = parent_inode;
    }

    /* Add entry to parent directory */
    if (ramfs_dir_add_entry(parent, filename, new_inode, INODE_TYPE_FILE) < 0) {
        ramfs_free_inode(new_inode);
        return -1;
    }

    *inode_out = new_inode;
    return 0;
}

/**
 * Create a directory at the given path.
 *
 * Parent directory must exist.
 * Returns 0 on success, -1 on failure.
 */
int ramfs_create_dir(const char* path, uint32_t* inode_out) {
    if (!path || !inode_out || path[0] != '/') {
        return -1;
    }

    /* Find parent directory and dirname */
    const char* last_slash = path;
    for (const char* p = path; *p; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }

    char parent_path[RAMFS_MAX_PATH];
    char dirname[RAMFS_MAX_FILENAME];

    if (last_slash == path) {
        /* Directory in root */
        parent_path[0] = '/';
        parent_path[1] = '\0';
        strncpy(dirname, path + 1, RAMFS_MAX_FILENAME - 1);
        dirname[RAMFS_MAX_FILENAME - 1] = '\0';
    } else {
        /* Copy parent path */
        size_t parent_len = last_slash - path;
        if (parent_len >= RAMFS_MAX_PATH) {
            return -1;
        }
        memcpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';

        /* Copy dirname */
        strncpy(dirname, last_slash + 1, RAMFS_MAX_FILENAME - 1);
        dirname[RAMFS_MAX_FILENAME - 1] = '\0';
    }

    if (dirname[0] == '\0') {
        return -1;  /* Empty dirname */
    }

    /* Look up parent directory */
    uint32_t parent_inode;
    if (ramfs_lookup_path(parent_path, &parent_inode) < 0) {
        return -1;  /* Parent doesn't exist */
    }

    ramfs_inode_t* parent = ramfs_get_inode(parent_inode);
    if (!parent || parent->type != INODE_TYPE_DIR) {
        return -1;
    }

    /* Check if directory already exists */
    uint32_t existing;
    if (ramfs_dir_lookup(parent, dirname, &existing) == 0) {
        return -1;  /* Already exists */
    }

    /* Allocate new inode */
    uint32_t new_inode;
    if (ramfs_alloc_inode(INODE_TYPE_DIR, &new_inode) < 0) {
        return -1;
    }

    /* Set parent and link count */
    ramfs_inode_t* inode = ramfs_get_inode(new_inode);
    if (inode) {
        inode->parent = parent_inode;
        inode->link_count = 2;  /* . and parent */
    }

    /* Increment parent's link count for .. reference */
    parent->link_count++;

    /* Add entry to parent directory */
    if (ramfs_dir_add_entry(parent, dirname, new_inode, INODE_TYPE_DIR) < 0) {
        ramfs_free_inode(new_inode);
        parent->link_count--;
        return -1;
    }

    *inode_out = new_inode;
    return 0;
}
