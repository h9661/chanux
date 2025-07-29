/*
 * Heap Allocator Header
 * 
 * This file defines the interface for the kernel heap allocator.
 * The heap provides dynamic memory allocation (malloc/free) for
 * kernel use. It uses a first-fit algorithm with coalescing.
 */

#ifndef _HEAP_H
#define _HEAP_H

#include <stdint.h>
#include <stddef.h>

/* Heap configuration */
#define HEAP_START      0x04000000  /* Heap starts at 64MB */
#define HEAP_INITIAL_SIZE 0x100000  /* Initial heap size: 1MB */
#define HEAP_MAX_SIZE   0x04000000  /* Maximum heap size: 64MB */
#define HEAP_MIN_BLOCK_SIZE 16      /* Minimum allocatable block */
#define HEAP_ALIGNMENT  8           /* Memory alignment requirement */

/* Magic numbers for heap integrity checking */
#define HEAP_MAGIC      0x12345678  /* Valid heap block */
#define HEAP_DEAD       0xDEADBEEF  /* Freed block marker */

/* Block header structure
 * Each allocated/free block has this header
 */
typedef struct heap_block {
    uint32_t magic;             /* Magic number for validation */
    size_t size;                /* Size of data area (not including header) */
    struct heap_block* next;    /* Next block in list */
    struct heap_block* prev;    /* Previous block in list */
    uint32_t free;              /* 1 if free, 0 if allocated */
} heap_block_t;

/* Heap statistics structure */
typedef struct heap_stats {
    size_t total_size;          /* Total heap size */
    size_t used_size;           /* Bytes currently allocated */
    size_t free_size;           /* Bytes currently free */
    size_t num_allocations;     /* Number of active allocations */
    size_t num_frees;           /* Total number of frees */
    size_t largest_free_block;  /* Size of largest free block */
} heap_stats_t;

/* Initialize the heap allocator */
void heap_init(void);

/* Allocate memory from the heap */
void* malloc(size_t size);

/* Free memory back to the heap */
void free(void* ptr);

/* Allocate and zero memory */
void* calloc(size_t num, size_t size);

/* Reallocate memory block */
void* realloc(void* ptr, size_t new_size);

/* Get heap statistics */
heap_stats_t heap_get_stats(void);

/* Check heap integrity */
int heap_check_integrity(void);

/* Print heap blocks (for debugging) */
void heap_print_blocks(void);

/* Expand heap size (internal use) */
int heap_expand(size_t additional_size);

/* Find best fit block (internal use) */
heap_block_t* heap_find_block(size_t size);

/* Split a block if it's too large (internal use) */
void heap_split_block(heap_block_t* block, size_t size);

/* Coalesce adjacent free blocks (internal use) */
void heap_coalesce(heap_block_t* block);

/* Align size to heap alignment (internal use) */
static inline size_t heap_align(size_t size) {
    return (size + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
}

#endif /* _HEAP_H */