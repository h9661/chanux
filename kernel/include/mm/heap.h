/**
 * =============================================================================
 * Chanux OS - Kernel Heap Header
 * =============================================================================
 * First-fit memory allocator with block coalescing for kernel use.
 *
 * The heap is located at virtual address 0xFFFFFFFF81000000 and can grow
 * up to 240MB. Physical pages are allocated on demand via the PMM and
 * mapped via the VMM.
 * =============================================================================
 */

#ifndef CHANUX_HEAP_H
#define CHANUX_HEAP_H

#include "../kernel.h"
#include "../types.h"

/* =============================================================================
 * Heap Configuration
 * =============================================================================
 */

#define HEAP_START          0xFFFFFFFF81000000ULL   /* Virtual start address */
#define HEAP_INITIAL_SIZE   (4 * 1024 * 1024)       /* 4MB initial size */
#define HEAP_MAX_SIZE       (240 * 1024 * 1024)     /* 240MB maximum */
#define HEAP_EXPAND_SIZE    (1 * 1024 * 1024)       /* 1MB expansion increment */

#define HEAP_MIN_BLOCK      32              /* Minimum allocation size */
#define HEAP_ALIGNMENT      16              /* Allocation alignment */

/* Block header magic number for validation */
#define HEAP_BLOCK_MAGIC    0xDEADBEEFUL

/* Block flags */
#define HEAP_BLOCK_FREE     0x0
#define HEAP_BLOCK_USED     0x1

/* =============================================================================
 * Heap Block Structure
 * =============================================================================
 */

/**
 * Block header structure
 * Placed before each allocation in memory.
 * Total size: 40 bytes (must be aligned to 16 bytes)
 */
typedef struct heap_block {
    uint32_t magic;             /* Validation magic number */
    uint32_t flags;             /* HEAP_BLOCK_FREE or HEAP_BLOCK_USED */
    size_t size;                /* Size of data area (excluding header) */
    struct heap_block* next;    /* Next block in memory */
    struct heap_block* prev;    /* Previous block in memory */
} PACKED ALIGNED(16) heap_block_t;

/* Header size (must be multiple of HEAP_ALIGNMENT) */
#define HEAP_HEADER_SIZE    ALIGN_UP(sizeof(heap_block_t), HEAP_ALIGNMENT)

/* =============================================================================
 * Heap Statistics
 * =============================================================================
 */

typedef struct {
    size_t total_size;          /* Total heap size */
    size_t used_size;           /* Currently allocated */
    size_t free_size;           /* Available space */
    size_t largest_free;        /* Largest free block */
    size_t block_count;         /* Total number of blocks */
    size_t free_block_count;    /* Number of free blocks */
    uint64_t alloc_count;       /* Total allocations */
    uint64_t free_count;        /* Total frees */
} heap_stats_t;

/* =============================================================================
 * Heap API
 * =============================================================================
 */

/**
 * Initialize the kernel heap
 * Maps initial heap pages and sets up the free list.
 */
void heap_init(void);

/**
 * Allocate memory from the kernel heap
 *
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void* kmalloc(size_t size);

/**
 * Allocate zeroed memory from the kernel heap
 *
 * @param size Number of bytes to allocate
 * @return Pointer to zeroed memory, or NULL on failure
 */
void* kzalloc(size_t size);

/**
 * Allocate aligned memory from the kernel heap
 *
 * @param size Number of bytes to allocate
 * @param alignment Required alignment (must be power of 2)
 * @return Pointer to aligned memory, or NULL on failure
 */
void* kmalloc_aligned(size_t size, size_t alignment);

/**
 * Free memory allocated by kmalloc
 *
 * @param ptr Pointer to memory to free (can be NULL)
 */
void kfree(void* ptr);

/**
 * Reallocate memory
 *
 * @param ptr Pointer to existing allocation (can be NULL)
 * @param new_size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 */
void* krealloc(void* ptr, size_t new_size);

/**
 * Expand the heap by allocating more pages
 *
 * @param min_size Minimum amount to expand by
 * @return true on success, false if heap is at maximum size
 */
bool heap_expand(size_t min_size);

/**
 * Get heap statistics
 *
 * @param stats Pointer to stats structure to fill
 */
void heap_get_stats(heap_stats_t* stats);

/**
 * Validate heap integrity
 * Checks all block headers for corruption.
 *
 * @return true if heap is valid, false if corruption detected
 */
bool heap_validate(void);

/**
 * Print heap debug information
 */
void heap_debug_print(void);

#endif /* CHANUX_HEAP_H */
