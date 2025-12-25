/**
 * =============================================================================
 * Chanux OS - Kernel Heap Implementation
 * =============================================================================
 * First-fit memory allocator with block coalescing.
 * =============================================================================
 */

#include "../include/mm/heap.h"
#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/mm/mm.h"
#include "../include/string.h"
#include "../drivers/vga/vga.h"

/* =============================================================================
 * Heap Internal State
 * =============================================================================
 */

/* First block in the heap */
static heap_block_t* heap_first = NULL;

/* Current heap size and break (end address) */
static size_t heap_size = 0;
static virt_addr_t heap_break = HEAP_START;

/* Statistics */
static uint64_t heap_alloc_count = 0;
static uint64_t heap_free_count = 0;

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/* Get the data area from a block */
static inline void* block_to_ptr(heap_block_t* block) {
    return (void*)((uint8_t*)block + HEAP_HEADER_SIZE);
}

/* Get the block from a data pointer */
static inline heap_block_t* ptr_to_block(void* ptr) {
    return (heap_block_t*)((uint8_t*)ptr - HEAP_HEADER_SIZE);
}

/* Check if block is valid */
static inline bool block_valid(heap_block_t* block) {
    return block && block->magic == HEAP_BLOCK_MAGIC;
}

/* Split a block if it's large enough */
static void block_split(heap_block_t* block, size_t size) {
    /* Check if there's enough space for a new block */
    size_t remaining = block->size - size - HEAP_HEADER_SIZE;
    if (remaining < HEAP_MIN_BLOCK) {
        return;  /* Not enough space to split */
    }

    /* Create new block after the current one */
    heap_block_t* new_block = (heap_block_t*)((uint8_t*)block + HEAP_HEADER_SIZE + size);
    new_block->magic = HEAP_BLOCK_MAGIC;
    new_block->flags = HEAP_BLOCK_FREE;
    new_block->size = remaining;
    new_block->prev = block;
    new_block->next = block->next;

    if (block->next) {
        block->next->prev = new_block;
    }

    block->next = new_block;
    block->size = size;
}

/* Merge with next block if it's free */
static void block_merge_next(heap_block_t* block) {
    if (!block->next) return;
    if (block->next->flags != HEAP_BLOCK_FREE) return;
    if (!block_valid(block->next)) return;

    heap_block_t* next = block->next;

    /* Absorb next block */
    block->size += HEAP_HEADER_SIZE + next->size;
    block->next = next->next;

    if (next->next) {
        next->next->prev = block;
    }

    /* Clear the merged block's magic */
    next->magic = 0;
}

/* =============================================================================
 * Heap Initialization
 * =============================================================================
 */

void heap_init(void) {
    kprintf("[HEAP] Initializing kernel heap...\n");
    kprintf("[HEAP] Virtual address: 0x%08x%08x\n",
            (uint32_t)(HEAP_START >> 32), (uint32_t)HEAP_START);
    kprintf("[HEAP] Initial size: %d KB\n", HEAP_INITIAL_SIZE / 1024);

    /* Map initial heap pages */
    for (size_t offset = 0; offset < HEAP_INITIAL_SIZE; offset += PAGE_SIZE) {
        phys_addr_t page = pmm_alloc_page();
        if (page == 0) {
            PANIC("Cannot allocate heap pages");
        }

        if (!vmm_map_page(HEAP_START + offset, page, PTE_KERNEL_RW)) {
            PANIC("Cannot map heap pages");
        }
    }

    /* Initialize first free block */
    heap_first = (heap_block_t*)HEAP_START;
    heap_first->magic = HEAP_BLOCK_MAGIC;
    heap_first->flags = HEAP_BLOCK_FREE;
    heap_first->size = HEAP_INITIAL_SIZE - HEAP_HEADER_SIZE;
    heap_first->next = NULL;
    heap_first->prev = NULL;

    heap_size = HEAP_INITIAL_SIZE;
    heap_break = HEAP_START + HEAP_INITIAL_SIZE;

    kprintf("[HEAP] Initialization complete.\n");
    kprintf("[HEAP] Usable space: %d KB\n",
            (uint32_t)(heap_first->size / 1024));
}

/* =============================================================================
 * Memory Allocation
 * =============================================================================
 */

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    /* Align size */
    size = ALIGN_UP(size, HEAP_ALIGNMENT);
    if (size < HEAP_MIN_BLOCK) {
        size = HEAP_MIN_BLOCK;
    }

    /* First-fit search */
    heap_block_t* block = heap_first;
    while (block) {
        if (!block_valid(block)) {
            kprintf("[HEAP] ERROR: Corrupted block at 0x%p\n", block);
            return NULL;
        }

        if (block->flags == HEAP_BLOCK_FREE && block->size >= size) {
            /* Found suitable block */
            block_split(block, size);
            block->flags = HEAP_BLOCK_USED;
            heap_alloc_count++;
            return block_to_ptr(block);
        }

        block = block->next;
    }

    /* No suitable block found - try to expand heap */
    size_t expand_size = MAX(size + HEAP_HEADER_SIZE, HEAP_EXPAND_SIZE);
    if (!heap_expand(expand_size)) {
        kprintf("[HEAP] ERROR: Out of memory (requested %d bytes)\n", (int)size);
        return NULL;
    }

    /* Retry allocation after expansion */
    return kmalloc(size);
}

void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* kmalloc_aligned(size_t size, size_t alignment) {
    /* Simple approach: over-allocate and adjust pointer */
    if (alignment <= HEAP_ALIGNMENT) {
        return kmalloc(size);
    }

    /* Allocate extra space for alignment */
    size_t total = size + alignment + sizeof(void*);
    void* ptr = kmalloc(total);
    if (!ptr) return NULL;

    /* Calculate aligned address */
    uintptr_t addr = (uintptr_t)ptr + sizeof(void*);
    uintptr_t aligned = ALIGN_UP(addr, alignment);

    /* Store original pointer before aligned address */
    ((void**)aligned)[-1] = ptr;

    return (void*)aligned;
}

/* =============================================================================
 * Memory Deallocation
 * =============================================================================
 */

void kfree(void* ptr) {
    if (!ptr) return;

    /* Check if this is an aligned allocation */
    /* (This is a simplification - a real implementation would track this) */

    heap_block_t* block = ptr_to_block(ptr);

    /* Validate block */
    if (!block_valid(block)) {
        kprintf("[HEAP] ERROR: Invalid free at 0x%p\n", ptr);
        return;
    }

    if (block->flags != HEAP_BLOCK_USED) {
        kprintf("[HEAP] WARNING: Double free at 0x%p\n", ptr);
        return;
    }

    /* Mark as free */
    block->flags = HEAP_BLOCK_FREE;
    heap_free_count++;

    /* Coalesce with adjacent free blocks */
    block_merge_next(block);

    if (block->prev && block->prev->flags == HEAP_BLOCK_FREE) {
        block_merge_next(block->prev);
    }
}

void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return kmalloc(new_size);
    }

    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    heap_block_t* block = ptr_to_block(ptr);
    if (!block_valid(block)) {
        return NULL;
    }

    /* If new size fits in current block, just return */
    new_size = ALIGN_UP(new_size, HEAP_ALIGNMENT);
    if (new_size <= block->size) {
        return ptr;
    }

    /* Try to expand into next free block */
    if (block->next && block->next->flags == HEAP_BLOCK_FREE) {
        size_t combined = block->size + HEAP_HEADER_SIZE + block->next->size;
        if (combined >= new_size) {
            block_merge_next(block);
            block_split(block, new_size);
            return ptr;
        }
    }

    /* Allocate new block and copy */
    void* new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, block->size);
    kfree(ptr);

    return new_ptr;
}

/* =============================================================================
 * Heap Expansion
 * =============================================================================
 */

bool heap_expand(size_t min_size) {
    /* Align to page size */
    size_t expand_size = ALIGN_UP(min_size, PAGE_SIZE);

    /* Check if we can expand */
    if (heap_size + expand_size > HEAP_MAX_SIZE) {
        kprintf("[HEAP] Cannot expand: would exceed maximum size\n");
        return false;
    }

    kprintf("[HEAP] Expanding by %d KB...\n", (int)(expand_size / 1024));

    /* Map new pages */
    for (size_t offset = 0; offset < expand_size; offset += PAGE_SIZE) {
        phys_addr_t page = pmm_alloc_page();
        if (page == 0) {
            kprintf("[HEAP] Expansion failed: out of physical memory\n");
            return false;
        }

        if (!vmm_map_page(heap_break + offset, page, PTE_KERNEL_RW)) {
            pmm_free_page(page);
            kprintf("[HEAP] Expansion failed: cannot map page\n");
            return false;
        }
    }

    /* Create new free block at end of heap */
    heap_block_t* new_block = (heap_block_t*)heap_break;
    new_block->magic = HEAP_BLOCK_MAGIC;
    new_block->flags = HEAP_BLOCK_FREE;
    new_block->size = expand_size - HEAP_HEADER_SIZE;
    new_block->next = NULL;
    new_block->prev = NULL;

    /* Find last block and link */
    heap_block_t* last = heap_first;
    while (last->next) {
        last = last->next;
    }

    last->next = new_block;
    new_block->prev = last;

    /* Try to merge with previous if it's free */
    if (last->flags == HEAP_BLOCK_FREE) {
        block_merge_next(last);
    }

    heap_break += expand_size;
    heap_size += expand_size;

    return true;
}

/* =============================================================================
 * Statistics and Debug
 * =============================================================================
 */

void heap_get_stats(heap_stats_t* stats) {
    if (!stats) return;

    memset(stats, 0, sizeof(heap_stats_t));
    stats->total_size = heap_size;
    stats->alloc_count = heap_alloc_count;
    stats->free_count = heap_free_count;

    heap_block_t* block = heap_first;
    while (block) {
        if (!block_valid(block)) break;

        stats->block_count++;

        if (block->flags == HEAP_BLOCK_FREE) {
            stats->free_size += block->size;
            stats->free_block_count++;
            if (block->size > stats->largest_free) {
                stats->largest_free = block->size;
            }
        } else {
            stats->used_size += block->size;
        }

        block = block->next;
    }
}

bool heap_validate(void) {
    heap_block_t* block = heap_first;
    heap_block_t* prev = NULL;

    while (block) {
        /* Check magic */
        if (block->magic != HEAP_BLOCK_MAGIC) {
            kprintf("[HEAP] Validation FAILED: bad magic at 0x%p\n", block);
            return false;
        }

        /* Check prev pointer */
        if (block->prev != prev) {
            kprintf("[HEAP] Validation FAILED: bad prev at 0x%p\n", block);
            return false;
        }

        /* Check flags */
        if (block->flags != HEAP_BLOCK_FREE && block->flags != HEAP_BLOCK_USED) {
            kprintf("[HEAP] Validation FAILED: bad flags at 0x%p\n", block);
            return false;
        }

        prev = block;
        block = block->next;
    }

    return true;
}

void heap_debug_print(void) {
    heap_stats_t stats;
    heap_get_stats(&stats);

    kprintf("\n[HEAP] Kernel Heap Statistics:\n");
    kprintf("  Total size:      %d KB\n", (uint32_t)(stats.total_size / 1024));
    kprintf("  Used:            %d KB\n", (uint32_t)(stats.used_size / 1024));
    kprintf("  Free:            %d KB\n", (uint32_t)(stats.free_size / 1024));
    kprintf("  Largest free:    %d KB\n", (uint32_t)(stats.largest_free / 1024));
    kprintf("  Total blocks:    %d\n", (uint32_t)stats.block_count);
    kprintf("  Free blocks:     %d\n", (uint32_t)stats.free_block_count);
    kprintf("  Allocations:     %d\n", (uint32_t)stats.alloc_count);
    kprintf("  Frees:           %d\n", (uint32_t)stats.free_count);
}
