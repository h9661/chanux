# Memory Management Restructuring Design for User Mode Support

## Executive Summary

This document outlines the architectural design for restructuring ChanUX's memory management system to support user mode processes with proper memory isolation, protection, and management capabilities. The design maintains compatibility with the 1GB RAM constraint while providing secure and efficient memory management for both kernel and user space.

## Design Goals

1. **Memory Isolation**: Complete separation between kernel and user space memory
2. **Security**: Prevent unauthorized access to kernel memory from user mode
3. **Efficiency**: Optimize for the 1GB RAM constraint
4. **Scalability**: Support multiple concurrent user processes
5. **Compatibility**: Maintain existing kernel functionality

## Architecture Overview

### Memory Layout

```
Physical Memory (1GB)
┌─────────────────────────┐ 0xFFFFFFFF (4GB)
│                         │
│    Unused (3GB)         │
│                         │
├─────────────────────────┤ 0x40000000 (1GB)
│    User Space           │
│    (768MB)              │
│                         │
│  ┌───────────────────┐  │
│  │ Process N Stack   │  │
│  ├───────────────────┤  │
│  │ Process N Heap    │  │
│  ├───────────────────┤  │
│  │ Process N Code    │  │
│  ├───────────────────┤  │
│  │       ...         │  │
│  ├───────────────────┤  │
│  │ Process 1 Stack   │  │
│  ├───────────────────┤  │
│  │ Process 1 Heap    │  │
│  ├───────────────────┤  │
│  │ Process 1 Code    │  │
│  └───────────────────┘  │
│                         │
├─────────────────────────┤ 0x10000000 (256MB)
│    Kernel Heap          │
│    (128MB)              │
├─────────────────────────┤ 0x08000000 (128MB)
│    Kernel Data/BSS      │
│    (64MB)               │
├─────────────────────────┤ 0x04000000 (64MB)
│    Kernel Code          │
│    (60MB)               │
├─────────────────────────┤ 0x00400000 (4MB)
│    Page Tables          │
│    (2MB)                │
├─────────────────────────┤ 0x00200000 (2MB)
│    PMM Bitmap           │
│    (1MB)                │
├─────────────────────────┤ 0x00100000 (1MB)
│    BIOS/Reserved        │
│    (1MB)                │
└─────────────────────────┘ 0x00000000

Virtual Memory Layout (Per Process)
┌─────────────────────────┐ 0xFFFFFFFF
│    Kernel Space         │
│    (Not Accessible)     │
├─────────────────────────┤ 0xC0000000 (3GB)
│    User Stack           │
│    (grows down)         │
│         ↓               │
│                         │
│    Unmapped Region      │
│                         │
│         ↑               │
│    User Heap            │
│    (grows up)           │
├─────────────────────────┤ Variable (after data)
│    User Data/BSS        │
├─────────────────────────┤ Variable
│    User Code            │
├─────────────────────────┤ 0x08048000 (typical)
│    Reserved             │
└─────────────────────────┘ 0x00000000
```

## Component Design

### 1. Enhanced Physical Memory Manager (PMM)

#### New Features
- **Page type tracking**: Differentiate kernel, user, and shared pages
- **Reference counting**: Track page usage across processes
- **NUMA awareness**: Optimize for cache locality (future)
- **Memory statistics**: Track usage per process

#### Data Structures
```c
typedef enum {
    PAGE_TYPE_FREE = 0,
    PAGE_TYPE_KERNEL = 1,
    PAGE_TYPE_USER = 2,
    PAGE_TYPE_SHARED = 3,
    PAGE_TYPE_RESERVED = 4
} page_type_t;

typedef struct page {
    uint32_t ref_count;
    page_type_t type;
    uint16_t flags;
    struct page *next;  // For free list
} page_t;

typedef struct pmm_stats {
    uint32_t total_pages;
    uint32_t free_pages;
    uint32_t kernel_pages;
    uint32_t user_pages;
    uint32_t shared_pages;
} pmm_stats_t;
```

### 2. Virtual Memory Manager (VMM) Redesign

#### Key Enhancements
- **Proper privilege separation**: Enforce PAGE_USER flag
- **Copy-on-write (COW)**: Efficient process forking
- **Demand paging**: Load pages on first access
- **Memory region tracking**: Virtual Memory Areas (VMAs)

#### Virtual Memory Area (VMA) System
```c
typedef struct vma {
    uint32_t start;        // Start virtual address
    uint32_t end;          // End virtual address
    uint32_t flags;        // Protection flags
    uint32_t type;         // Code, data, stack, heap, etc.
    struct vma *next;      // Linked list
    struct vma *prev;
    
    // File mapping info (for mmap)
    struct file *file;
    uint32_t file_offset;
} vma_t;

typedef struct mm_struct {
    page_directory_t *pgdir;
    vma_t *vma_list;       // Sorted list of VMAs
    uint32_t vma_count;
    
    // Memory usage statistics
    uint32_t total_vm;     // Total virtual memory
    uint32_t locked_vm;    // Locked pages
    uint32_t shared_vm;    // Shared pages
    
    // Heap boundaries
    uint32_t brk;          // Current heap end
    uint32_t start_brk;    // Heap start
    
    // Stack info
    uint32_t start_stack;  // Stack bottom
    uint32_t stack_limit;  // Stack size limit
} mm_struct_t;
```

### 3. User Space Memory Allocator

#### Design Approach
- **Syscall interface**: brk(), sbrk(), mmap(), munmap()
- **Per-process heap**: Isolated heap management
- **Memory mapped files**: Support for file-backed memory
- **Anonymous mappings**: Private memory allocations

#### System Calls
```c
// Memory allocation
void* sys_brk(void *addr);
void* sys_sbrk(intptr_t increment);

// Memory mapping
void* sys_mmap(void *addr, size_t length, int prot, 
               int flags, int fd, off_t offset);
int sys_munmap(void *addr, size_t length);

// Memory protection
int sys_mprotect(void *addr, size_t len, int prot);

// Memory locking
int sys_mlock(const void *addr, size_t len);
int sys_munlock(const void *addr, size_t len);
```

### 4. Process Memory Layout Manager

#### Standard Process Layout
```c
#define USER_CODE_BASE    0x08048000  // Standard ELF load address
#define USER_STACK_TOP    0xC0000000  // 3GB mark
#define USER_STACK_SIZE   0x00800000  // 8MB default
#define USER_HEAP_GAP     0x01000000  // 16MB gap between heap/stack

typedef struct process_memory_layout {
    // Code segment
    uint32_t code_start;
    uint32_t code_end;
    
    // Data segment
    uint32_t data_start;
    uint32_t data_end;
    
    // BSS segment
    uint32_t bss_start;
    uint32_t bss_end;
    
    // Heap
    uint32_t heap_start;
    uint32_t heap_current;
    uint32_t heap_limit;
    
    // Stack
    uint32_t stack_top;
    uint32_t stack_bottom;
    
    // Memory mapped regions
    uint32_t mmap_base;
} process_memory_layout_t;
```

### 5. Memory Protection Mechanisms

#### Page Protection Flags
```c
#define PROT_NONE     0x0    // No access
#define PROT_READ     0x1    // Read permission
#define PROT_WRITE    0x2    // Write permission
#define PROT_EXEC     0x4    // Execute permission
#define PROT_KERNEL   0x8    // Kernel only access

// Enhanced page flags
#define PAGE_USER     0x004  // User accessible
#define PAGE_RW       0x002  // Read/Write
#define PAGE_PRESENT  0x001  // Present in memory
#define PAGE_NX       0x80000000  // No execute (if supported)
#define PAGE_COW      0x200  // Copy-on-write
#define PAGE_SHARED   0x400  // Shared between processes
```

#### Access Validation
```c
// Validate user pointer before kernel access
bool validate_user_pointer(void *ptr, size_t size, int access);

// Copy data safely between user/kernel space
int copy_from_user(void *dest, const void *src, size_t n);
int copy_to_user(void *dest, const void *src, size_t n);

// String operations with user space
int strncpy_from_user(char *dest, const char *src, size_t n);
```

## Implementation Phases

### Phase 1: Memory Isolation (2-3 weeks)
1. **Week 1**: Implement page protection enforcement
   - Update VMM to respect PAGE_USER flag
   - Add access validation to page fault handler
   - Implement kernel/user boundary checks

2. **Week 2**: User pointer validation
   - Implement validate_user_pointer()
   - Add copy_from_user/copy_to_user functions
   - Update all syscalls to validate user pointers

3. **Week 3**: Testing and hardening
   - Create test suite for memory isolation
   - Fix privilege escalation vulnerabilities
   - Performance optimization

### Phase 2: User Memory Management (3-4 weeks)
1. **Week 1**: VMA system implementation
   - Implement VMA data structures
   - Add VMA management functions
   - Integrate with process structure

2. **Week 2**: Basic memory syscalls
   - Implement brk/sbrk syscalls
   - Add simple mmap for anonymous memory
   - Update process creation for proper layout

3. **Week 3**: Advanced memory features
   - Implement file-backed mmap
   - Add mprotect for changing permissions
   - Implement memory statistics tracking

4. **Week 4**: Integration and testing
   - Integrate with existing components
   - Comprehensive testing suite
   - Performance benchmarking

### Phase 3: Advanced Features (4-5 weeks)
1. **Week 1**: Copy-on-write implementation
   - Add COW page marking
   - Implement COW fault handler
   - Update fork() to use COW

2. **Week 2**: Demand paging
   - Implement lazy allocation
   - Add page-in mechanism
   - Optimize for common patterns

3. **Week 3**: Shared memory
   - Implement shared memory syscalls
   - Add IPC shared memory support
   - Security considerations

4. **Week 4-5**: Optimization and polish
   - Performance profiling
   - Memory defragmentation
   - Documentation and examples

## Security Considerations

### Memory Access Control
1. **Privilege separation**: Strict kernel/user boundary
2. **Stack protection**: Guard pages and canaries
3. **ASLR preparation**: Randomizable memory layout
4. **W^X enforcement**: Separate write and execute permissions

### Validation Requirements
1. **All syscalls**: Validate user pointers before access
2. **Page tables**: Verify user cannot modify kernel pages
3. **Memory limits**: Enforce per-process memory quotas
4. **Resource exhaustion**: Prevent memory DoS attacks

## Performance Optimizations

### TLB Management
- **Selective invalidation**: Only flush modified entries
- **ASID support**: Prepare for tagged TLBs (future)
- **Batch operations**: Group TLB operations

### Memory Allocation
- **Free list optimization**: Per-size free lists
- **Page clustering**: Allocate contiguous pages
- **NUMA awareness**: Local memory preference

### Copy-on-Write
- **Reference counting**: Efficient page sharing
- **Lazy copying**: Defer until write
- **Fork optimization**: Share read-only pages

## Testing Strategy

### Unit Tests
1. **PMM tests**: Allocation, deallocation, fragmentation
2. **VMM tests**: Mapping, protection, page faults
3. **VMA tests**: Region management, overlaps, splits
4. **Syscall tests**: All memory syscalls

### Integration Tests
1. **Process creation**: Memory layout verification
2. **Fork testing**: COW functionality
3. **Memory pressure**: Behavior under low memory
4. **Security tests**: Isolation verification

### Performance Tests
1. **Allocation speed**: malloc/free benchmarks
2. **Page fault latency**: Demand paging performance
3. **Fork performance**: COW efficiency
4. **Memory bandwidth**: Copy performance

## Migration Strategy

### Backward Compatibility
- **Kernel compatibility**: Existing kernel code unchanged
- **Gradual migration**: Phase-based implementation
- **Feature flags**: Enable/disable new features

### Risk Mitigation
1. **Incremental changes**: Small, testable commits
2. **Rollback plan**: Ability to revert changes
3. **Extensive testing**: Each phase fully tested
4. **Documentation**: Clear upgrade guides

## Success Criteria

### Functional Requirements
- ✓ Complete kernel/user memory isolation
- ✓ Working memory allocation syscalls
- ✓ Process memory layout management
- ✓ Copy-on-write fork implementation
- ✓ Memory protection enforcement

### Performance Requirements
- ✓ Fork latency < 1ms for small processes
- ✓ Page fault handling < 10μs
- ✓ Memory allocation < 100ns for small sizes
- ✓ Less than 5% overhead vs direct access

### Security Requirements
- ✓ No kernel memory access from user mode
- ✓ All user pointers validated
- ✓ Stack overflow protection
- ✓ Memory exhaustion prevention

## Conclusion

This design provides a comprehensive roadmap for implementing user mode memory management in ChanUX. The phased approach allows for incremental development while maintaining system stability. The architecture balances security, performance, and memory efficiency within the 1GB constraint, providing a solid foundation for user mode process support.