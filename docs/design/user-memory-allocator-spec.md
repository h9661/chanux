# User Space Memory Allocator Specification

## Overview

This document specifies the design and implementation of user space memory allocation in ChanUX, including heap management, memory mapping, and the system call interface.

## Architecture

### Memory Allocation Hierarchy

```
User Application
    ↓
C Library (malloc/free)
    ↓
System Calls (brk/mmap)
    ↓
Kernel VMA Manager
    ↓
Virtual Memory Manager
    ↓
Physical Memory Manager
```

## System Call Interface

### 1. Heap Management (brk/sbrk)

```c
// src/kernel/syscalls/sys_memory.c

// Set program break (heap end)
void* sys_brk(void *addr) {
    process_t *proc = current_process;
    mm_struct_t *mm = proc->mm;
    
    // If addr is NULL, return current break
    if (!addr) {
        return (void*)mm->brk;
    }
    
    uint32_t new_brk = PAGE_ALIGN((uint32_t)addr);
    
    // Validate new break
    if (new_brk < mm->start_brk) {
        return (void*)mm->brk;  // Cannot shrink below start
    }
    
    if (new_brk > mm->start_brk + MAX_HEAP_SIZE) {
        return (void*)mm->brk;  // Heap size limit
    }
    
    // Check if it would overlap with stack
    if (new_brk > mm->start_stack - MIN_STACK_GAP) {
        return (void*)mm->brk;  // Too close to stack
    }
    
    // Growing heap
    if (new_brk > mm->brk) {
        if (!expand_heap(mm, mm->brk, new_brk)) {
            return (void*)mm->brk;  // Failed to expand
        }
    }
    // Shrinking heap
    else if (new_brk < mm->brk) {
        shrink_heap(mm, new_brk, mm->brk);
    }
    
    mm->brk = new_brk;
    return (void*)new_brk;
}

// Increment program break
void* sys_sbrk(intptr_t increment) {
    process_t *proc = current_process;
    void *old_brk = (void*)proc->mm->brk;
    
    if (increment == 0) {
        return old_brk;
    }
    
    void *new_brk = sys_brk((char*)old_brk + increment);
    
    if (new_brk == old_brk && increment != 0) {
        return (void*)-1;  // Failed
    }
    
    return old_brk;
}

// Helper: Expand heap
static bool expand_heap(mm_struct_t *mm, uint32_t old_brk, uint32_t new_brk) {
    // Find or create heap VMA
    vma_t *heap_vma = find_vma_containing(mm, old_brk - 1);
    
    if (!heap_vma || heap_vma->type != VMA_HEAP) {
        // Create new heap VMA
        heap_vma = create_vma(mm->start_brk, new_brk, 
                            PROT_READ | PROT_WRITE,
                            VMA_HEAP | VMA_GROWSUP);
        if (!heap_vma)
            return false;
            
        insert_vma(mm, heap_vma);
    } else {
        // Extend existing heap VMA
        heap_vma->end = new_brk;
    }
    
    // Allocate pages for new heap region
    for (uint32_t addr = old_brk; addr < new_brk; addr += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_user_page();
        if (!frame) {
            // Rollback on failure
            for (uint32_t rollback = old_brk; rollback < addr; rollback += PAGE_SIZE) {
                page_table_entry_t *pte = get_page_entry(mm->pgdir, rollback);
                if (pte && pte->present) {
                    pmm_free_page(pte->frame << 12);
                    pte->present = 0;
                }
            }
            return false;
        }
        
        // Map page as user-accessible
        vmm_map_page(mm->pgdir, addr, frame,
                    PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }
    
    // Update memory statistics
    mm->total_vm += (new_brk - old_brk) / PAGE_SIZE;
    
    return true;
}
```

### 2. Memory Mapping (mmap/munmap)

```c
// src/kernel/syscalls/sys_mmap.c

// Memory mapping flags
#define MAP_SHARED      0x01    // Share changes
#define MAP_PRIVATE     0x02    // Changes are private
#define MAP_FIXED       0x10    // Use exact address
#define MAP_ANONYMOUS   0x20    // Not backed by file
#define MAP_GROWSDOWN   0x0100  // Stack-like segment
#define MAP_LOCKED      0x2000  // Lock in memory
#define MAP_POPULATE    0x8000  // Prefault pages

// Map memory into process address space
void* sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    process_t *proc = current_process;
    mm_struct_t *mm = proc->mm;
    
    // Validate parameters
    if (length == 0 || length > MAX_MMAP_SIZE) {
        return MAP_FAILED;
    }
    
    // Align length to page boundary
    length = PAGE_ALIGN(length);
    
    // Validate protection flags
    if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) {
        return MAP_FAILED;
    }
    
    // MAP_SHARED and MAP_PRIVATE are mutually exclusive
    if (!(flags & (MAP_SHARED | MAP_PRIVATE)) ||
        (flags & MAP_SHARED && flags & MAP_PRIVATE)) {
        return MAP_FAILED;
    }
    
    // Handle address hint
    uint32_t map_addr = (uint32_t)addr;
    if (flags & MAP_FIXED) {
        // Must use exact address
        if (!IS_ALIGNED(map_addr, PAGE_SIZE)) {
            return MAP_FAILED;
        }
        
        // Check if region is available
        if (!is_region_available(mm, map_addr, length)) {
            return MAP_FAILED;
        }
    } else {
        // Find suitable address
        map_addr = find_free_region(mm, length, map_addr);
        if (!map_addr) {
            return MAP_FAILED;
        }
    }
    
    // Create VMA for mapped region
    vma_t *vma = create_vma(map_addr, map_addr + length, prot, 
                           VMA_MMAP | (flags & MAP_SHARED ? VMA_SHARED : 0));
    if (!vma) {
        return MAP_FAILED;
    }
    
    // Handle file mapping
    if (!(flags & MAP_ANONYMOUS)) {
        file_t *file = get_file_from_fd(fd);
        if (!file) {
            free_vma(vma);
            return MAP_FAILED;
        }
        
        vma->file = file;
        vma->file_offset = offset;
        file->ref_count++;
    }
    
    // Insert VMA into process
    insert_vma(mm, vma);
    
    // Populate pages if requested
    if (flags & MAP_POPULATE) {
        populate_vma(mm, vma);
    }
    
    // Update statistics
    mm->total_vm += length / PAGE_SIZE;
    if (flags & MAP_SHARED) {
        mm->shared_vm += length / PAGE_SIZE;
    }
    
    return (void*)map_addr;
}

// Unmap memory region
int sys_munmap(void *addr, size_t length) {
    process_t *proc = current_process;
    mm_struct_t *mm = proc->mm;
    
    uint32_t start = (uint32_t)addr;
    uint32_t end = start + length;
    
    // Validate parameters
    if (!IS_ALIGNED(start, PAGE_SIZE) || length == 0) {
        return -EINVAL;
    }
    
    // Find all VMAs in range
    vma_t *vma = mm->vma_list;
    while (vma && vma->start < end) {
        if (vma->end > start) {
            // This VMA overlaps with unmap region
            unmap_vma_range(mm, vma, start, end);
        }
        vma = vma->next;
    }
    
    return 0;
}

// Helper: Unmap part of VMA
static void unmap_vma_range(mm_struct_t *mm, vma_t *vma, 
                           uint32_t start, uint32_t end) {
    uint32_t unmap_start = MAX(vma->start, start);
    uint32_t unmap_end = MIN(vma->end, end);
    
    // Unmap pages
    for (uint32_t addr = unmap_start; addr < unmap_end; addr += PAGE_SIZE) {
        page_table_entry_t *pte = get_page_entry(mm->pgdir, addr);
        if (pte && pte->present) {
            // Handle different page types
            if (vma->flags & VMA_SHARED) {
                // Decrease reference count for shared pages
                pmm_unref_page(pte->frame << 12);
            } else {
                // Free private pages
                pmm_free_page(pte->frame << 12);
            }
            
            // Clear PTE
            pte->value = 0;
            tlb_invalidate_page(addr);
        }
    }
    
    // Update statistics
    uint32_t pages = (unmap_end - unmap_start) / PAGE_SIZE;
    mm->total_vm -= pages;
    if (vma->flags & VMA_SHARED) {
        mm->shared_vm -= pages;
    }
    
    // Handle VMA splitting/removal
    if (unmap_start > vma->start && unmap_end < vma->end) {
        // Split VMA into two
        vma_t *new_vma = create_vma(unmap_end, vma->end, 
                                   vma->flags, vma->type);
        new_vma->file = vma->file;
        new_vma->file_offset = vma->file_offset + (unmap_end - vma->start);
        
        vma->end = unmap_start;
        insert_vma_after(mm, vma, new_vma);
    } else if (unmap_start <= vma->start && unmap_end >= vma->end) {
        // Remove entire VMA
        remove_vma(mm, vma);
        free_vma(vma);
    } else if (unmap_start <= vma->start) {
        // Trim from beginning
        vma->start = unmap_end;
        if (vma->file) {
            vma->file_offset += (unmap_end - vma->start);
        }
    } else {
        // Trim from end
        vma->end = unmap_start;
    }
}
```

### 3. Memory Protection (mprotect)

```c
// src/kernel/syscalls/sys_mprotect.c

// Change memory protection
int sys_mprotect(void *addr, size_t len, int prot) {
    process_t *proc = current_process;
    mm_struct_t *mm = proc->mm;
    
    uint32_t start = (uint32_t)addr;
    uint32_t end = start + len;
    
    // Validate alignment
    if (!IS_ALIGNED(start, PAGE_SIZE)) {
        return -EINVAL;
    }
    
    // Find all VMAs in range
    vma_t *vma = find_vma(mm, start);
    while (vma && vma->start < end) {
        if (vma->end > start) {
            // Update VMA protection
            change_vma_protection(mm, vma, start, end, prot);
        }
        vma = vma->next;
    }
    
    return 0;
}

// Helper: Change protection for VMA range
static void change_vma_protection(mm_struct_t *mm, vma_t *vma,
                                 uint32_t start, uint32_t end, int prot) {
    uint32_t change_start = MAX(vma->start, start);
    uint32_t change_end = MIN(vma->end, end);
    
    // Update page table entries
    for (uint32_t addr = change_start; addr < change_end; addr += PAGE_SIZE) {
        page_table_entry_t *pte = get_page_entry(mm->pgdir, addr);
        if (pte && pte->present) {
            // Update protection flags
            pte->rw = (prot & PROT_WRITE) ? 1 : 0;
            pte->user = 1;  // Always user accessible
            
            // Handle execute protection (if supported)
            if (cpu_has_nx()) {
                pte->nx = (prot & PROT_EXEC) ? 0 : 1;
            }
            
            tlb_invalidate_page(addr);
        }
    }
    
    // Update VMA flags if entire VMA changed
    if (change_start <= vma->start && change_end >= vma->end) {
        vma->prot = prot;
    }
}
```

## Virtual Memory Area (VMA) Management

```c
// src/kernel/memory/vma.c

// VMA structure
typedef struct vma {
    uint32_t start;        // Start address
    uint32_t end;          // End address (exclusive)
    uint32_t prot;         // Protection flags
    uint32_t flags;        // VMA flags
    
    // For file mappings
    struct file *file;
    uint32_t file_offset;
    
    // Red-black tree for efficient lookup
    struct rb_node rb_node;
    
    // Linked list
    struct vma *next;
    struct vma *prev;
} vma_t;

// VMA flags
#define VMA_HEAP        0x01
#define VMA_STACK       0x02
#define VMA_MMAP        0x04
#define VMA_SHARED      0x08
#define VMA_GROWSDOWN   0x10
#define VMA_GROWSUP     0x20
#define VMA_LOCKED      0x40

// Find VMA containing address
vma_t* find_vma(mm_struct_t *mm, uint32_t addr) {
    struct rb_node *node = mm->vma_tree.rb_node;
    
    while (node) {
        vma_t *vma = rb_entry(node, vma_t, rb_node);
        
        if (addr < vma->start) {
            node = node->rb_left;
        } else if (addr >= vma->end) {
            node = node->rb_right;
        } else {
            return vma;
        }
    }
    
    return NULL;
}

// Find free region for mapping
uint32_t find_free_region(mm_struct_t *mm, size_t length, uint32_t hint) {
    uint32_t addr = hint ? hint : mm->mmap_base;
    vma_t *vma = mm->vma_list;
    
    // Align to page boundary
    addr = PAGE_ALIGN(addr);
    
    // Search for gap between VMAs
    while (vma) {
        if (addr + length <= vma->start) {
            // Found suitable gap
            return addr;
        }
        
        addr = PAGE_ALIGN(vma->end);
        vma = vma->next;
    }
    
    // Check if there's space after last VMA
    if (addr + length <= USER_SPACE_END) {
        return addr;
    }
    
    return 0;  // No suitable region found
}

// Insert VMA into sorted list and tree
void insert_vma(mm_struct_t *mm, vma_t *new_vma) {
    struct rb_node **link = &mm->vma_tree.rb_node;
    struct rb_node *parent = NULL;
    vma_t *vma;
    
    // Find position in red-black tree
    while (*link) {
        parent = *link;
        vma = rb_entry(parent, vma_t, rb_node);
        
        if (new_vma->end <= vma->start) {
            link = &parent->rb_left;
        } else if (new_vma->start >= vma->end) {
            link = &parent->rb_right;
        } else {
            // Overlapping VMAs - should not happen
            panic("VMA overlap detected");
        }
    }
    
    // Insert into tree
    rb_link_node(&new_vma->rb_node, parent, link);
    rb_insert_color(&new_vma->rb_node, &mm->vma_tree);
    
    // Insert into sorted linked list
    vma_t *prev = NULL;
    vma_t *curr = mm->vma_list;
    
    while (curr && curr->start < new_vma->start) {
        prev = curr;
        curr = curr->next;
    }
    
    new_vma->next = curr;
    new_vma->prev = prev;
    
    if (prev) {
        prev->next = new_vma;
    } else {
        mm->vma_list = new_vma;
    }
    
    if (curr) {
        curr->prev = new_vma;
    }
    
    mm->vma_count++;
}
```

## Page Fault Handling for User Memory

```c
// src/kernel/memory/user_fault.c

// Handle page fault in user space
bool handle_user_fault(uint32_t fault_addr, uint32_t error_code) {
    process_t *proc = current_process;
    mm_struct_t *mm = proc->mm;
    vma_t *vma;
    
    // Find VMA for faulting address
    vma = find_vma(mm, fault_addr);
    if (!vma) {
        // No VMA - segmentation fault
        return false;
    }
    
    // Check permissions
    bool write_fault = error_code & 0x2;
    bool user_mode = error_code & 0x4;
    
    if (write_fault && !(vma->prot & PROT_WRITE)) {
        // Write to read-only region
        return false;
    }
    
    // Handle different fault types
    if (vma->flags & VMA_GROWSDOWN) {
        // Stack growth
        return handle_stack_growth(mm, vma, fault_addr);
    } else if (vma->file) {
        // File-backed mapping
        return handle_file_fault(mm, vma, fault_addr);
    } else {
        // Anonymous mapping - allocate zero page
        return handle_anon_fault(mm, vma, fault_addr);
    }
}

// Handle anonymous page fault
static bool handle_anon_fault(mm_struct_t *mm, vma_t *vma, uint32_t addr) {
    // Allocate new page
    uint32_t frame = pmm_alloc_user_page();
    if (!frame) {
        return false;  // Out of memory
    }
    
    // Zero the page
    memset((void*)frame, 0, PAGE_SIZE);
    
    // Map page
    uint32_t flags = PAGE_PRESENT | PAGE_USER;
    if (vma->prot & PROT_WRITE) {
        flags |= PAGE_RW;
    }
    
    vmm_map_page(mm->pgdir, addr & PAGE_MASK, frame, flags);
    
    return true;
}

// Handle file-backed page fault
static bool handle_file_fault(mm_struct_t *mm, vma_t *vma, uint32_t addr) {
    uint32_t page_offset = addr & PAGE_MASK;
    uint32_t file_offset = vma->file_offset + (page_offset - vma->start);
    
    // Allocate page
    uint32_t frame = pmm_alloc_user_page();
    if (!frame) {
        return false;
    }
    
    // Read file data into page
    size_t bytes_read = vfs_read_at(vma->file, (void*)frame, 
                                   PAGE_SIZE, file_offset);
    
    // Zero remaining bytes
    if (bytes_read < PAGE_SIZE) {
        memset((void*)(frame + bytes_read), 0, PAGE_SIZE - bytes_read);
    }
    
    // Map page
    uint32_t flags = PAGE_PRESENT | PAGE_USER;
    if (vma->prot & PROT_WRITE) {
        if (vma->flags & VMA_SHARED) {
            flags |= PAGE_RW;
        } else {
            // Private mapping - mark for COW
            flags |= PAGE_COW;
        }
    }
    
    vmm_map_page(mm->pgdir, page_offset, frame, flags);
    
    return true;
}
```

## C Library Interface Example

```c
// Example minimal malloc implementation for user space
// This would be part of the C library, not kernel

#define HEAP_EXTEND_SIZE    (64 * 1024)  // Extend heap by 64KB

typedef struct block {
    size_t size;
    struct block *next;
    struct block *prev;
    int free;
} block_t;

static block_t *heap_start = NULL;
static void *heap_end = NULL;

void* malloc(size_t size) {
    block_t *block;
    
    // Align size
    size = (size + 7) & ~7;
    
    // Initialize heap on first call
    if (!heap_start) {
        heap_start = heap_end = sbrk(0);
    }
    
    // Search for free block
    block = heap_start;
    while (block < (block_t*)heap_end) {
        if (block->free && block->size >= size) {
            // Found suitable block
            block->free = 0;
            
            // Split if too large
            if (block->size > size + sizeof(block_t) + 16) {
                block_t *new_block = (block_t*)((char*)block + 
                                               sizeof(block_t) + size);
                new_block->size = block->size - size - sizeof(block_t);
                new_block->free = 1;
                new_block->next = block->next;
                new_block->prev = block;
                
                if (block->next) {
                    block->next->prev = new_block;
                }
                block->next = new_block;
                block->size = size;
            }
            
            return (char*)block + sizeof(block_t);
        }
        
        block = block->next;
    }
    
    // No suitable block - extend heap
    void *prev_heap_end = heap_end;
    heap_end = sbrk(size + sizeof(block_t));
    
    if (heap_end == (void*)-1) {
        return NULL;  // Out of memory
    }
    
    // Create new block
    block = (block_t*)prev_heap_end;
    block->size = size;
    block->free = 0;
    block->next = NULL;
    block->prev = NULL;
    
    // Link to previous block
    if (prev_heap_end > heap_start) {
        block_t *last = heap_start;
        while (last->next) {
            last = last->next;
        }
        last->next = block;
        block->prev = last;
    }
    
    return (char*)block + sizeof(block_t);
}

void free(void *ptr) {
    if (!ptr) return;
    
    block_t *block = (block_t*)((char*)ptr - sizeof(block_t));
    block->free = 1;
    
    // Coalesce with next block if free
    if (block->next && block->next->free) {
        block->size += sizeof(block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    
    // Coalesce with previous block if free
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(block_t) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}
```

## Testing and Validation

```c
// tests/user_memory_test.c

void test_brk_sbrk(void) {
    // Test basic brk functionality
    void *initial_brk = sbrk(0);
    assert(initial_brk != (void*)-1);
    
    // Extend heap
    void *new_brk = sbrk(4096);
    assert(new_brk == initial_brk);
    assert(sbrk(0) == (char*)initial_brk + 4096);
    
    // Write to allocated memory
    strcpy(new_brk, "Hello, heap!");
    assert(strcmp(new_brk, "Hello, heap!") == 0);
    
    // Shrink heap
    brk(initial_brk);
    assert(sbrk(0) == initial_brk);
}

void test_mmap_munmap(void) {
    // Test anonymous mapping
    void *addr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(addr != MAP_FAILED);
    
    // Write to mapped memory
    strcpy(addr, "Hello, mmap!");
    assert(strcmp(addr, "Hello, mmap!") == 0);
    
    // Unmap
    assert(munmap(addr, 4096) == 0);
}

void test_mprotect(void) {
    // Map memory as read-write
    void *addr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(addr != MAP_FAILED);
    
    // Write data
    *(int*)addr = 42;
    
    // Change to read-only
    assert(mprotect(addr, 4096, PROT_READ) == 0);
    
    // Reading should work
    assert(*(int*)addr == 42);
    
    // Writing should fail (catch signal in real test)
    // *(int*)addr = 24;  // Should cause segfault
    
    munmap(addr, 4096);
}
```

## Performance Considerations

### Optimization Strategies

1. **VMA Caching**: Cache recently accessed VMAs
2. **Bulk Page Allocation**: Allocate multiple pages at once
3. **Lazy Allocation**: Defer physical page allocation until access
4. **Page Clustering**: Keep related pages physically contiguous
5. **NUMA Awareness**: Allocate memory close to CPU

### Expected Performance

- brk() system call: ~500 cycles
- mmap() anonymous: ~1000 cycles  
- Page fault handling: ~2000 cycles
- malloc() fast path: ~50 cycles
- free() fast path: ~30 cycles