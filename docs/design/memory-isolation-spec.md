# Memory Isolation Implementation Specification

## Overview

This document provides detailed implementation specifications for establishing memory isolation between kernel and user space in ChanUX OS.

## Core Isolation Mechanisms

### 1. Page Table Permission Enforcement

#### Updated Page Directory Entry Structure
```c
// src/kernel/memory/paging.h
typedef struct page_directory_entry {
    uint32_t present    : 1;   // Page present in memory
    uint32_t rw         : 1;   // Read/Write (0=read-only, 1=read/write)
    uint32_t user       : 1;   // User/Supervisor (0=kernel, 1=user)
    uint32_t pwt        : 1;   // Page-level write-through
    uint32_t pcd        : 1;   // Page-level cache disable
    uint32_t accessed   : 1;   // Accessed flag
    uint32_t dirty      : 1;   // Dirty flag (for pages)
    uint32_t ps         : 1;   // Page size (0=4KB, 1=4MB)
    uint32_t global     : 1;   // Global page (not flushed on CR3 reload)
    uint32_t cow        : 1;   // Copy-on-write flag (available bit)
    uint32_t shared     : 1;   // Shared page flag (available bit)
    uint32_t reserved   : 1;   // Reserved for future use
    uint32_t frame      : 20;  // Physical frame address (4KB aligned)
} __attribute__((packed)) page_directory_entry_t;
```

#### Permission Checking in Page Fault Handler
```c
// src/kernel/memory/page_fault.c
void page_fault_handler(registers_t *regs) {
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));
    
    // Extract error code bits
    int present = regs->err_code & 0x1;
    int rw = regs->err_code & 0x2;
    int user = regs->err_code & 0x4;
    int reserved = regs->err_code & 0x8;
    int id = regs->err_code & 0x10;
    
    // User mode access to kernel page - SECURITY VIOLATION
    if (user && !is_user_accessible(faulting_address)) {
        kprintf("Security violation: User access to kernel memory at 0x%x\n", 
                faulting_address);
        process_terminate(current_process, SIGKILL);
        return;
    }
    
    // Handle other page fault cases...
}

bool is_user_accessible(uint32_t vaddr) {
    page_directory_t *dir = current_process->mm->pgdir;
    uint32_t dir_idx = vaddr >> 22;
    uint32_t table_idx = (vaddr >> 12) & 0x3FF;
    
    if (!dir->entries[dir_idx].present)
        return false;
        
    page_table_t *table = (page_table_t*)(dir->entries[dir_idx].frame << 12);
    page_table_entry_t *page = &table->entries[table_idx];
    
    return page->present && page->user;
}
```

### 2. Kernel/User Boundary Definition

```c
// src/kernel/memory/memory_layout.h
#define KERNEL_SPACE_START    0x00000000
#define KERNEL_SPACE_END      0xC0000000  // 3GB mark
#define USER_SPACE_START      0x08048000  // Standard ELF load address
#define USER_SPACE_END        0xC0000000  // 3GB mark

// Validate address is in user space
static inline bool is_user_address(void *addr) {
    uint32_t uaddr = (uint32_t)addr;
    return uaddr >= USER_SPACE_START && uaddr < USER_SPACE_END;
}

// Validate address range is entirely in user space
static inline bool is_user_range(void *addr, size_t size) {
    uint32_t start = (uint32_t)addr;
    uint32_t end = start + size;
    
    // Check for overflow
    if (end < start)
        return false;
        
    return start >= USER_SPACE_START && end <= USER_SPACE_END;
}
```

### 3. Safe User/Kernel Data Transfer

```c
// src/kernel/memory/user_access.c

// Copy data from user space to kernel space safely
int copy_from_user(void *kernel_dest, const void *user_src, size_t n) {
    // Validate user pointer
    if (!is_user_range((void*)user_src, n))
        return -EFAULT;
    
    // Temporarily allow access to user pages
    uint32_t old_cr0 = read_cr0();
    write_cr0(old_cr0 & ~CR0_WP);  // Disable write protect
    
    // Use page fault handler to catch any issues
    __try {
        memcpy(kernel_dest, user_src, n);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        write_cr0(old_cr0);  // Restore write protect
        return -EFAULT;
    }
    
    write_cr0(old_cr0);  // Restore write protect
    return 0;
}

// Copy data from kernel space to user space safely
int copy_to_user(void *user_dest, const void *kernel_src, size_t n) {
    // Validate user pointer
    if (!is_user_range(user_dest, n))
        return -EFAULT;
    
    // Ensure destination pages are writable
    if (!verify_user_write(user_dest, n))
        return -EFAULT;
    
    uint32_t old_cr0 = read_cr0();
    write_cr0(old_cr0 & ~CR0_WP);
    
    __try {
        memcpy(user_dest, kernel_src, n);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        write_cr0(old_cr0);
        return -EFAULT;
    }
    
    write_cr0(old_cr0);
    return 0;
}

// Safely copy string from user space
int strncpy_from_user(char *dest, const char *src, size_t n) {
    size_t i;
    
    if (!is_user_address((void*)src))
        return -EFAULT;
    
    for (i = 0; i < n; i++) {
        if (!is_user_address((void*)(src + i)))
            return -EFAULT;
            
        __try {
            dest[i] = src[i];
            if (src[i] == '\0')
                return i;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return -EFAULT;
        }
    }
    
    dest[n-1] = '\0';
    return n-1;
}
```

### 4. System Call Pointer Validation

```c
// src/kernel/process/syscall_validation.c

// Generic user pointer validation
bool validate_user_pointer(const void *ptr, size_t size, int access) {
    if (!ptr)
        return false;
        
    if (!is_user_range((void*)ptr, size))
        return false;
    
    // Check page permissions
    uint32_t addr = (uint32_t)ptr;
    uint32_t end_addr = addr + size;
    
    for (uint32_t page = addr & PAGE_MASK; page < end_addr; page += PAGE_SIZE) {
        page_table_entry_t *pte = get_page_entry(page);
        
        if (!pte || !pte->present || !pte->user)
            return false;
            
        if ((access & PROT_WRITE) && !pte->rw)
            return false;
    }
    
    return true;
}

// Updated system call with validation
int sys_write(int fd, const void *buf, size_t count) {
    // Validate user buffer
    if (!validate_user_pointer(buf, count, PROT_READ)) {
        return -EFAULT;
    }
    
    // Allocate kernel buffer
    void *kbuf = kmalloc(count);
    if (!kbuf)
        return -ENOMEM;
    
    // Safely copy from user space
    if (copy_from_user(kbuf, buf, count) != 0) {
        kfree(kbuf);
        return -EFAULT;
    }
    
    // Perform actual write
    int result = vfs_write(fd, kbuf, count);
    
    kfree(kbuf);
    return result;
}
```

### 5. Stack Protection

```c
// src/kernel/process/stack_protection.c

// Stack guard page size
#define STACK_GUARD_SIZE    PAGE_SIZE

// Allocate user stack with guard page
void* allocate_user_stack(size_t size) {
    // Allocate stack + guard page
    size_t total_size = size + STACK_GUARD_SIZE;
    uint32_t stack_top = USER_SPACE_END;
    
    // Find free region for stack
    vma_t *stack_vma = vma_find_free_region(current_process->mm, 
                                           total_size, 
                                           stack_top - total_size,
                                           stack_top);
    if (!stack_vma)
        return NULL;
    
    // Map stack pages (but not guard page)
    for (uint32_t addr = stack_vma->start + STACK_GUARD_SIZE; 
         addr < stack_vma->end; 
         addr += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_page();
        if (!frame) {
            // Cleanup and fail
            vma_unmap_region(stack_vma);
            return NULL;
        }
        
        vmm_map_page(current_process->mm->pgdir, addr, frame,
                    PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }
    
    // Guard page remains unmapped
    stack_vma->flags = VMA_STACK | VMA_GROWSDOWN;
    
    return (void*)stack_vma->end;
}

// Handle stack growth on page fault
bool handle_stack_growth(uint32_t fault_addr) {
    vma_t *stack_vma = find_vma(current_process->mm, fault_addr);
    
    if (!stack_vma || !(stack_vma->flags & VMA_GROWSDOWN))
        return false;
    
    // Check if fault is near stack
    if (fault_addr < stack_vma->start - MAX_STACK_GROWTH)
        return false;
    
    // Allocate new page
    uint32_t frame = pmm_alloc_page();
    if (!frame)
        return false;
    
    // Map the page
    vmm_map_page(current_process->mm->pgdir, 
                fault_addr & PAGE_MASK, 
                frame,
                PAGE_PRESENT | PAGE_RW | PAGE_USER);
    
    // Update VMA
    stack_vma->start = fault_addr & PAGE_MASK;
    
    return true;
}
```

### 6. Kernel Stack Protection

```c
// src/kernel/process/kernel_stack.c

// Kernel stack with overflow detection
typedef struct kernel_stack {
    uint32_t magic_bottom;  // Stack underflow detection
    uint8_t guard_page[PAGE_SIZE];  // Guard page
    uint8_t stack[KERNEL_STACK_SIZE - PAGE_SIZE];  // Actual stack
    uint32_t magic_top;     // Stack overflow detection
} kernel_stack_t;

#define STACK_MAGIC_BOTTOM  0xDEADBEEF
#define STACK_MAGIC_TOP     0xBEEFDEAD

// Allocate kernel stack for process
void* allocate_kernel_stack(void) {
    kernel_stack_t *kstack = (kernel_stack_t*)pmm_alloc_pages(
        (sizeof(kernel_stack_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    
    if (!kstack)
        return NULL;
    
    // Set up magic values
    kstack->magic_bottom = STACK_MAGIC_BOTTOM;
    kstack->magic_top = STACK_MAGIC_TOP;
    
    // Make guard page non-present
    uint32_t guard_vaddr = (uint32_t)kstack->guard_page;
    page_table_entry_t *pte = get_page_entry(guard_vaddr);
    pte->present = 0;
    
    // Return top of stack
    return &kstack->stack[KERNEL_STACK_SIZE - PAGE_SIZE];
}

// Check kernel stack integrity
void check_kernel_stack_integrity(process_t *proc) {
    kernel_stack_t *kstack = (kernel_stack_t*)
        ((uint32_t)proc->kernel_stack - offsetof(kernel_stack_t, stack));
    
    if (kstack->magic_bottom != STACK_MAGIC_BOTTOM) {
        panic("Kernel stack underflow detected for PID %d", proc->pid);
    }
    
    if (kstack->magic_top != STACK_MAGIC_TOP) {
        panic("Kernel stack overflow detected for PID %d", proc->pid);
    }
}
```

## Testing Strategy

### Security Tests

```c
// tests/memory_isolation_test.c

void test_user_kernel_isolation(void) {
    // Test 1: User process cannot read kernel memory
    if (fork() == 0) {
        // Child process (user mode)
        uint32_t *kernel_addr = (uint32_t*)0x100000;  // Kernel address
        
        // This should cause segmentation fault
        uint32_t value = *kernel_addr;  // Should fail
        
        // Should not reach here
        exit(1);
    }
    
    // Parent waits and checks child was killed
    int status;
    wait(&status);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
}

void test_syscall_validation(void) {
    // Test 2: System calls validate user pointers
    
    // Pass kernel pointer to syscall - should fail
    int ret = write(1, (void*)0x100000, 100);
    assert(ret == -EFAULT);
    
    // Pass invalid user pointer - should fail
    ret = write(1, (void*)0xFFFFFFFF, 100);
    assert(ret == -EFAULT);
    
    // Pass valid user pointer - should succeed
    char buf[] = "Hello";
    ret = write(1, buf, 5);
    assert(ret == 5);
}

void test_stack_protection(void) {
    // Test 3: Stack overflow detection
    if (fork() == 0) {
        // Cause stack overflow
        char large_array[1024 * 1024 * 10];  // 10MB array
        large_array[0] = 'A';  // Should fault
        exit(1);
    }
    
    int status;
    wait(&status);
    assert(WIFSIGNALED(status));
}
```

## Migration Checklist

### Phase 1: Basic Isolation
- [ ] Update page fault handler with security checks
- [ ] Implement user address validation functions  
- [ ] Add copy_from_user/copy_to_user functions
- [ ] Update all system calls to validate pointers
- [ ] Add stack guard pages
- [ ] Implement kernel stack overflow detection
- [ ] Create comprehensive test suite
- [ ] Performance benchmarking

### Phase 2: Enhanced Protection
- [ ] Implement W^X enforcement
- [ ] Add ASLR preparation
- [ ] Implement stack canaries
- [ ] Add memory access logging
- [ ] Security audit all kernel interfaces

## Performance Considerations

### Optimization Strategies
1. **TLB efficiency**: Minimize flushes by batching page table updates
2. **Fast path validation**: Cache validation results for repeated accesses
3. **Copy optimization**: Use SSE/AVX for large copies when available
4. **Lazy validation**: Defer validation until actual access for some operations

### Expected Overhead
- User pointer validation: ~10-20 cycles per check
- copy_from/to_user: ~50-100 cycles overhead vs memcpy
- Page fault handling: ~1000 cycles for security checks
- Overall system impact: 2-5% performance overhead