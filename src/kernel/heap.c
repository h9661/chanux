/*
 * Heap Allocator Implementation
 * 
 * This file implements a simple heap allocator using a linked list
 * of free blocks with first-fit allocation and coalescing.
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/heap.h"
#include "../include/vmm.h"
#include "../include/string.h"

/* Global heap state */
static heap_block_t* heap_start = NULL;
static heap_block_t* heap_end = NULL;
static size_t heap_size = 0;
static size_t heap_used = 0;
static size_t num_allocations = 0;
static size_t num_frees = 0;

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_write_hex(uint32_t value);
extern void terminal_write_dec(uint32_t value);

/*
 * Initialize the heap allocator
 */
void heap_init(void) {
    terminal_writestring("Initializing heap allocator...\n");
    
    /* Get current page directory */
    page_directory_t* page_dir = vmm_get_current_directory();
    
    /* Allocate initial heap pages */
    uint32_t num_pages = HEAP_INITIAL_SIZE / PAGE_SIZE;
    uint32_t heap_virt = HEAP_START;
    
    terminal_writestring("Allocating ");
    terminal_write_dec(num_pages);
    terminal_writestring(" pages for heap at ");
    terminal_write_hex(heap_virt);
    terminal_writestring("\n");
    
    /* Map pages for heap */
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t virt_addr = heap_virt + (i * PAGE_SIZE);
        if (vmm_alloc_page(page_dir, virt_addr, PAGE_PRESENT | PAGE_WRITABLE) == 0) {
            terminal_writestring("Heap: Failed to allocate page!\n");
            return;
        }
    }
    
    /* Initialize the first block */
    heap_start = (heap_block_t*)HEAP_START;
    heap_start->magic = HEAP_MAGIC;
    heap_start->size = HEAP_INITIAL_SIZE - sizeof(heap_block_t);
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_start->free = 1;
    
    heap_end = heap_start;
    heap_size = HEAP_INITIAL_SIZE;
    heap_used = sizeof(heap_block_t);
    
    terminal_writestring("Heap initialized: ");
    terminal_write_dec(heap_size / 1024);
    terminal_writestring(" KB available\n");
}

/*
 * Find a suitable free block using first-fit algorithm
 */
heap_block_t* heap_find_block(size_t size) {
    heap_block_t* current = heap_start;
    
    while (current) {
        /* Check if block is valid */
        if (current->magic != HEAP_MAGIC) {
            terminal_writestring("Heap corruption detected!\n");
            return NULL;
        }
        
        /* Check if block is free and large enough */
        if (current->free && current->size >= size) {
            return current;
        }
        
        current = current->next;
    }
    
    return NULL;
}

/*
 * Split a block if it's significantly larger than needed
 */
void heap_split_block(heap_block_t* block, size_t size) {
    /* Only split if remaining size is worth it */
    size_t total_size = block->size;
    size_t remainder = total_size - size - sizeof(heap_block_t);
    
    if (remainder >= HEAP_MIN_BLOCK_SIZE) {
        /* Create new block */
        heap_block_t* new_block = (heap_block_t*)((uint8_t*)block + sizeof(heap_block_t) + size);
        new_block->magic = HEAP_MAGIC;
        new_block->size = remainder;
        new_block->free = 1;
        new_block->prev = block;
        new_block->next = block->next;
        
        /* Update links */
        if (block->next) {
            block->next->prev = new_block;
        } else {
            heap_end = new_block;
        }
        block->next = new_block;
        block->size = size;
    }
}

/*
 * Coalesce adjacent free blocks
 */
void heap_coalesce(heap_block_t* block) {
    if (!block || !block->free) {
        return;
    }
    
    /* Coalesce with next block if free */
    if (block->next && block->next->free) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        } else {
            heap_end = block;
        }
    }
    
    /* Coalesce with previous block if free */
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(heap_block_t) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        } else {
            heap_end = block->prev;
        }
    }
}

/*
 * Expand the heap by allocating more pages
 */
int heap_expand(size_t additional_size) {
    /* Align to page size */
    additional_size = (additional_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    /* Check if we can expand */
    if (heap_size + additional_size > HEAP_MAX_SIZE) {
        return -1;
    }
    
    /* Get current page directory */
    page_directory_t* page_dir = vmm_get_current_directory();
    
    /* Allocate new pages */
    uint32_t num_pages = additional_size / PAGE_SIZE;
    uint32_t heap_current_end = HEAP_START + heap_size;
    
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t virt_addr = heap_current_end + (i * PAGE_SIZE);
        if (vmm_alloc_page(page_dir, virt_addr, PAGE_PRESENT | PAGE_WRITABLE) == 0) {
            /* Failed to allocate - rollback */
            for (uint32_t j = 0; j < i; j++) {
                vmm_free_page(page_dir, heap_current_end + (j * PAGE_SIZE));
            }
            return -1;
        }
    }
    
    /* Create new free block at end of heap */
    heap_block_t* new_block = (heap_block_t*)heap_current_end;
    new_block->magic = HEAP_MAGIC;
    new_block->size = additional_size - sizeof(heap_block_t);
    new_block->free = 1;
    new_block->prev = heap_end;
    new_block->next = NULL;
    
    /* Link to existing heap */
    heap_end->next = new_block;
    heap_end = new_block;
    
    /* Update heap size */
    heap_size += additional_size;
    heap_used += sizeof(heap_block_t);
    
    /* Try to coalesce with previous block */
    heap_coalesce(new_block);
    
    return 0;
}

/*
 * Allocate memory from the heap
 */
void* malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    /* Align size */
    size = heap_align(size);
    
    /* Find a suitable block */
    heap_block_t* block = heap_find_block(size);
    
    /* If no block found, try to expand heap */
    if (!block) {
        size_t expand_size = size + sizeof(heap_block_t);
        if (expand_size < HEAP_INITIAL_SIZE) {
            expand_size = HEAP_INITIAL_SIZE;
        }
        
        if (heap_expand(expand_size) < 0) {
            return NULL;  /* Out of memory */
        }
        
        /* Try again */
        block = heap_find_block(size);
        if (!block) {
            return NULL;
        }
    }
    
    /* Split block if necessary */
    heap_split_block(block, size);
    
    /* Mark block as allocated */
    block->free = 0;
    
    /* Update statistics */
    heap_used += block->size;
    num_allocations++;
    
    /* Return pointer to data area */
    return (void*)((uint8_t*)block + sizeof(heap_block_t));
}

/*
 * Free memory back to the heap
 */
void free(void* ptr) {
    if (!ptr) {
        return;
    }
    
    /* Get block header */
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    
    /* Validate block */
    if (block->magic != HEAP_MAGIC) {
        terminal_writestring("Heap: Invalid block in free()!\n");
        return;
    }
    
    if (block->free) {
        terminal_writestring("Heap: Double free detected!\n");
        return;
    }
    
    /* Mark as free */
    block->free = 1;
    
    /* Update statistics */
    heap_used -= block->size;
    num_frees++;
    
    /* Coalesce with adjacent free blocks */
    heap_coalesce(block);
}

/*
 * Allocate and zero memory
 */
void* calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void* ptr = malloc(total_size);
    
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

/*
 * Reallocate memory block
 */
void* realloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return malloc(new_size);
    }
    
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }
    
    /* Get current block */
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    
    /* Validate block */
    if (block->magic != HEAP_MAGIC || block->free) {
        return NULL;
    }
    
    /* If new size fits in current block, we're done */
    size_t aligned_size = heap_align(new_size);
    if (aligned_size <= block->size) {
        /* Optionally split if much smaller */
        if (block->size - aligned_size >= HEAP_MIN_BLOCK_SIZE + sizeof(heap_block_t)) {
            heap_split_block(block, aligned_size);
        }
        return ptr;
    }
    
    /* Need to allocate new block */
    void* new_ptr = malloc(new_size);
    if (!new_ptr) {
        return NULL;
    }
    
    /* Copy old data */
    size_t copy_size = block->size < new_size ? block->size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    
    /* Free old block */
    free(ptr);
    
    return new_ptr;
}

/*
 * Get heap statistics
 */
heap_stats_t heap_get_stats(void) {
    heap_stats_t stats;
    stats.total_size = heap_size;
    stats.used_size = heap_used;
    stats.free_size = heap_size - heap_used;
    stats.num_allocations = num_allocations - num_frees;
    stats.num_frees = num_frees;
    
    /* Find largest free block */
    stats.largest_free_block = 0;
    heap_block_t* current = heap_start;
    while (current) {
        if (current->free && current->size > stats.largest_free_block) {
            stats.largest_free_block = current->size;
        }
        current = current->next;
    }
    
    return stats;
}

/*
 * Check heap integrity
 */
int heap_check_integrity(void) {
    heap_block_t* current = heap_start;
    heap_block_t* prev = NULL;
    size_t total_size = 0;
    
    while (current) {
        /* Check magic number */
        if (current->magic != HEAP_MAGIC) {
            terminal_writestring("Heap: Invalid magic at ");
            terminal_write_hex((uint32_t)current);
            terminal_writestring("\n");
            return 0;
        }
        
        /* Check prev pointer */
        if (current->prev != prev) {
            terminal_writestring("Heap: Invalid prev pointer at ");
            terminal_write_hex((uint32_t)current);
            terminal_writestring("\n");
            return 0;
        }
        
        /* Check bounds */
        if ((uint32_t)current < HEAP_START || 
            (uint32_t)current >= HEAP_START + heap_size) {
            terminal_writestring("Heap: Block out of bounds at ");
            terminal_write_hex((uint32_t)current);
            terminal_writestring("\n");
            return 0;
        }
        
        total_size += sizeof(heap_block_t) + current->size;
        prev = current;
        current = current->next;
    }
    
    /* Check total size */
    if (total_size != heap_size) {
        terminal_writestring("Heap: Size mismatch\n");
        return 0;
    }
    
    return 1;
}

/*
 * Print heap blocks for debugging
 */
void heap_print_blocks(void) {
    terminal_writestring("\nHeap blocks:\n");
    terminal_writestring("Address     Size      Status\n");
    terminal_writestring("--------------------------------\n");
    
    heap_block_t* current = heap_start;
    int block_num = 0;
    
    while (current) {
        terminal_write_hex((uint32_t)current);
        terminal_writestring("  ");
        terminal_write_dec(current->size);
        terminal_writestring(" bytes  ");
        terminal_writestring(current->free ? "FREE" : "USED");
        terminal_writestring("\n");
        
        current = current->next;
        block_num++;
    }
    
    terminal_writestring("Total blocks: ");
    terminal_write_dec(block_num);
    terminal_writestring("\n");
}