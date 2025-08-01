# Process Memory Layout Specification

## Overview

This document specifies the standard memory layout for user processes in ChanUX, including ELF loading, segment organization, and dynamic memory regions.

## Standard Process Memory Layout

```
┌─────────────────────────┐ 0xFFFFFFFF (4GB)
│                         │
│    Kernel Space         │ (Not accessible from user mode)
│                         │
├─────────────────────────┤ 0xC0000000 (3GB) - User Space Ceiling
│    Environment/Args     │ 
├─────────────────────────┤ 0xBFFFFFFF
│                         │
│    User Stack          │ (grows downward ↓)
│                         │
│    [Stack Guard Page]   │ (unmapped for overflow detection)
│                         │
├─────────────────────────┤ Variable (typically ~0xBF800000)
│                         │
│    Thread Stacks        │ (for multi-threaded processes)
│                         │
├─────────────────────────┤
│                         │
│    Memory Mapped        │ (mmap region - grows downward ↓)
│    Region               │
│                         │
├─────────────────────────┤ Variable (typically ~0xB0000000)
│                         │
│                         │
│    Unmapped Space       │ (available for growth)
│                         │
│                         │
├─────────────────────────┤ Variable (heap end)
│                         │
│    Heap                 │ (grows upward ↑)
│                         │
├─────────────────────────┤ Variable (after BSS)
│    BSS Segment          │ (uninitialized data)
├─────────────────────────┤ Variable
│    Data Segment         │ (initialized data)
├─────────────────────────┤ Variable
│    Read-only Data       │ (.rodata)
├─────────────────────────┤ Variable
│    Text Segment         │ (executable code)
├─────────────────────────┤ 0x08048000 (standard base)
│    ELF Header          │
├─────────────────────────┤ 0x08048000
│                         │
│    Reserved            │ (NULL page protection)
│                         │
└─────────────────────────┘ 0x00000000
```

## Process Control Block Memory Extensions

```c
// src/kernel/process/process.h

// Extended process structure for memory management
typedef struct process {
    // Existing fields...
    pid_t pid;
    char name[32];
    process_state_t state;
    
    // Memory management
    mm_struct_t *mm;              // Memory descriptor
    
    // Memory layout
    uint32_t start_code;          // Beginning of code segment
    uint32_t end_code;            // End of code segment
    uint32_t start_data;          // Beginning of data segment
    uint32_t end_data;            // End of data segment
    uint32_t start_brk;           // Beginning of heap
    uint32_t brk;                 // Current heap end
    uint32_t start_stack;         // Bottom of main stack
    uint32_t start_mmap;          // Base of mmap area
    
    // ELF information
    uint32_t entry_point;         // Program entry point
    uint32_t elf_interpreter;     // Dynamic linker path
    
    // Resource limits
    struct rlimit rlim[RLIM_NLIMITS];
    
    // Memory statistics
    uint32_t vm_size;             // Total virtual memory (KB)
    uint32_t vm_rss;              // Resident set size (KB)
    uint32_t vm_shared;           // Shared memory (KB)
    
} process_t;

// Memory descriptor
typedef struct mm_struct {
    // Page directory
    page_directory_t *pgdir;
    
    // VMA management
    vma_t *vma_list;              // List of VMAs
    struct rb_root vma_tree;      // RB-tree for fast lookup
    uint32_t vma_count;           // Number of VMAs
    
    // Memory boundaries
    uint32_t start_code, end_code;
    uint32_t start_data, end_data;
    uint32_t start_brk, brk;
    uint32_t start_stack;
    uint32_t start_mmap;
    
    // Memory statistics
    uint32_t total_vm;            // Total pages
    uint32_t locked_vm;           // Locked pages
    uint32_t shared_vm;           // Shared pages
    uint32_t exec_vm;             // Executable pages
    uint32_t stack_vm;            // Stack pages
    
    // Reference counting
    atomic_t mm_users;            // Users of this mm
    atomic_t mm_count;            // Reference count
    
    // Memory semaphore
    semaphore_t mmap_sem;         // Protects VMA list
    
} mm_struct_t;
```

## ELF Loading and Process Creation

```c
// src/kernel/exec/elf_loader.c

// ELF header structures
typedef struct {
    unsigned char e_ident[16];    // Magic number and other info
    uint16_t e_type;              // Object file type
    uint16_t e_machine;           // Architecture
    uint32_t e_version;           // Object file version
    uint32_t e_entry;             // Entry point virtual address
    uint32_t e_phoff;             // Program header table offset
    uint32_t e_shoff;             // Section header table offset
    uint32_t e_flags;             // Processor-specific flags
    uint16_t e_ehsize;            // ELF header size
    uint16_t e_phentsize;         // Program header table entry size
    uint16_t e_phnum;             // Program header table entry count
    uint16_t e_shentsize;         // Section header table entry size
    uint16_t e_shnum;             // Section header table entry count
    uint16_t e_shstrndx;          // Section header string table index
} elf32_header_t;

// Program header
typedef struct {
    uint32_t p_type;              // Segment type
    uint32_t p_offset;            // Segment file offset
    uint32_t p_vaddr;             // Segment virtual address
    uint32_t p_paddr;             // Segment physical address
    uint32_t p_filesz;            // Segment size in file
    uint32_t p_memsz;             // Segment size in memory
    uint32_t p_flags;             // Segment flags
    uint32_t p_align;             // Segment alignment
} elf32_phdr_t;

// Load ELF executable
int load_elf(const char *path, process_t *proc) {
    file_t *file = vfs_open(path, O_RDONLY);
    if (!file) {
        return -ENOENT;
    }
    
    // Read ELF header
    elf32_header_t ehdr;
    if (vfs_read(file, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        vfs_close(file);
        return -EIO;
    }
    
    // Validate ELF magic
    if (memcmp(ehdr.e_ident, "\x7fELF", 4) != 0) {
        vfs_close(file);
        return -ENOEXEC;
    }
    
    // Create new mm_struct
    mm_struct_t *mm = create_mm();
    if (!mm) {
        vfs_close(file);
        return -ENOMEM;
    }
    
    // Initialize memory layout
    mm->start_code = UINT32_MAX;
    mm->end_code = 0;
    mm->start_data = UINT32_MAX;
    mm->end_data = 0;
    
    // Load program segments
    for (int i = 0; i < ehdr.e_phnum; i++) {
        elf32_phdr_t phdr;
        vfs_seek(file, ehdr.e_phoff + i * sizeof(phdr), SEEK_SET);
        vfs_read(file, &phdr, sizeof(phdr));
        
        if (phdr.p_type == PT_LOAD) {
            if (load_segment(file, mm, &phdr) < 0) {
                destroy_mm(mm);
                vfs_close(file);
                return -ENOMEM;
            }
            
            // Update memory boundaries
            update_memory_boundaries(mm, &phdr);
        }
    }
    
    // Set up heap
    mm->start_brk = mm->brk = PAGE_ALIGN(mm->end_data);
    
    // Set up stack
    if (setup_user_stack(mm, proc) < 0) {
        destroy_mm(mm);
        vfs_close(file);
        return -ENOMEM;
    }
    
    // Set entry point
    proc->entry_point = ehdr.e_entry;
    
    // Attach mm to process
    proc->mm = mm;
    
    vfs_close(file);
    return 0;
}

// Load a program segment
static int load_segment(file_t *file, mm_struct_t *mm, elf32_phdr_t *phdr) {
    uint32_t start = phdr->p_vaddr;
    uint32_t end = start + phdr->p_memsz;
    uint32_t prot = 0;
    
    // Convert ELF flags to VMA protection
    if (phdr->p_flags & PF_R) prot |= PROT_READ;
    if (phdr->p_flags & PF_W) prot |= PROT_WRITE;
    if (phdr->p_flags & PF_X) prot |= PROT_EXEC;
    
    // Create VMA for segment
    vma_t *vma = create_vma(start, end, prot, VMA_FILE);
    if (!vma) {
        return -ENOMEM;
    }
    
    vma->file = file;
    vma->file_offset = phdr->p_offset;
    
    // Insert VMA
    insert_vma(mm, vma);
    
    // Map pages (demand paging - only map what's needed)
    if (phdr->p_filesz > 0) {
        // Map first page immediately
        uint32_t first_page = start & PAGE_MASK;
        if (map_elf_page(mm, vma, first_page) < 0) {
            remove_vma(mm, vma);
            return -ENOMEM;
        }
    }
    
    // Handle BSS (zero-initialized data)
    if (phdr->p_memsz > phdr->p_filesz) {
        uint32_t bss_start = start + phdr->p_filesz;
        uint32_t bss_end = end;
        
        // Zero the BSS portion of the last file page
        if (phdr->p_filesz > 0) {
            uint32_t last_file_page = (bss_start - 1) & PAGE_MASK;
            uint32_t offset_in_page = bss_start & ~PAGE_MASK;
            if (offset_in_page > 0) {
                // Zero from offset to end of page
                page_table_entry_t *pte = get_page_entry(mm->pgdir, last_file_page);
                if (pte && pte->present) {
                    memset((void*)(last_file_page + offset_in_page), 0,
                          PAGE_SIZE - offset_in_page);
                }
            }
        }
    }
    
    return 0;
}

// Update memory boundaries based on segment
static void update_memory_boundaries(mm_struct_t *mm, elf32_phdr_t *phdr) {
    uint32_t start = phdr->p_vaddr;
    uint32_t end = start + phdr->p_memsz;
    
    // Code segment (executable)
    if (phdr->p_flags & PF_X) {
        if (start < mm->start_code)
            mm->start_code = start;
        if (end > mm->end_code)
            mm->end_code = end;
    }
    // Data segment (writable)
    else if (phdr->p_flags & PF_W) {
        if (start < mm->start_data)
            mm->start_data = start;
        if (end > mm->end_data)
            mm->end_data = end;
    }
}
```

## Stack Setup and Management

```c
// src/kernel/exec/stack_setup.c

#define STACK_SIZE          (8 * 1024 * 1024)  // 8MB default stack
#define STACK_GUARD_SIZE    PAGE_SIZE          // 4KB guard page
#define ARG_MAX             (128 * 1024)       // Max args + environ

// Set up user stack with arguments and environment
int setup_user_stack(mm_struct_t *mm, process_t *proc, 
                    int argc, char *argv[], char *envp[]) {
    // Calculate stack placement
    uint32_t stack_top = USER_SPACE_END;
    uint32_t stack_bottom = stack_top - STACK_SIZE;
    
    // Create stack VMA
    vma_t *stack_vma = create_vma(stack_bottom, stack_top,
                                 PROT_READ | PROT_WRITE,
                                 VMA_STACK | VMA_GROWSDOWN);
    if (!stack_vma) {
        return -ENOMEM;
    }
    
    insert_vma(mm, stack_vma);
    
    // Map initial stack pages (top pages only)
    uint32_t initial_pages = 4;  // 16KB initial stack
    for (int i = 0; i < initial_pages; i++) {
        uint32_t addr = stack_top - (i + 1) * PAGE_SIZE;
        uint32_t frame = pmm_alloc_user_page();
        if (!frame) {
            return -ENOMEM;
        }
        
        vmm_map_page(mm->pgdir, addr, frame,
                    PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }
    
    // Set up stack contents
    uint32_t sp = stack_top;
    
    // Copy environment strings
    int envc = 0;
    char **envp_user = NULL;
    if (envp) {
        sp = copy_strings_to_stack(mm, envp, &sp, &envc);
        envp_user = (char**)(sp - (envc + 1) * sizeof(char*));
    }
    
    // Copy argument strings
    sp = copy_strings_to_stack(mm, argv, &sp, &argc);
    char **argv_user = (char**)(sp - (argc + 1) * sizeof(char*));
    
    // Align stack to 16 bytes
    sp &= ~0xF;
    
    // Build stack frame (x86 ABI)
    // Stack layout (growing downward):
    // [strings...]
    // NULL
    // envp[n-1]
    // ...
    // envp[0]
    // NULL
    // argv[n-1]
    // ...
    // argv[0]
    // argc
    
    // Push auxiliary vector (TODO: implement fully)
    sp -= sizeof(Elf32_auxv_t) * 2;
    Elf32_auxv_t *auxv = (Elf32_auxv_t*)sp;
    auxv[0].a_type = AT_NULL;
    auxv[0].a_un.a_val = 0;
    
    // Push environment pointers
    if (envp_user) {
        sp -= (envc + 1) * sizeof(char*);
        copy_to_user((void*)sp, envp_user, (envc + 1) * sizeof(char*));
    } else {
        sp -= sizeof(char*);
        uint32_t null = 0;
        copy_to_user((void*)sp, &null, sizeof(null));
    }
    
    // Push argument pointers
    sp -= (argc + 1) * sizeof(char*);
    copy_to_user((void*)sp, argv_user, (argc + 1) * sizeof(char*));
    
    // Push argc
    sp -= sizeof(int);
    copy_to_user((void*)sp, &argc, sizeof(argc));
    
    // Set stack pointer
    mm->start_stack = sp;
    proc->start_stack = stack_bottom;
    
    return sp;
}

// Copy strings to user stack
static uint32_t copy_strings_to_stack(mm_struct_t *mm, char *strs[],
                                     uint32_t *sp, int *count) {
    int n = 0;
    uint32_t str_sp = *sp;
    
    // Count strings
    if (strs) {
        while (strs[n]) n++;
    }
    *count = n;
    
    // Copy strings in reverse order
    char **str_addrs = kmalloc((n + 1) * sizeof(char*));
    for (int i = n - 1; i >= 0; i--) {
        int len = strlen(strs[i]) + 1;
        str_sp -= len;
        str_sp &= ~3;  // Align to 4 bytes
        
        copy_to_user((void*)str_sp, strs[i], len);
        str_addrs[i] = (char*)str_sp;
    }
    str_addrs[n] = NULL;
    
    // Copy string pointers
    str_sp -= (n + 1) * sizeof(char*);
    copy_to_user((void*)str_sp, str_addrs, (n + 1) * sizeof(char*));
    
    kfree(str_addrs);
    
    *sp = str_sp;
    return str_sp;
}
```

## Memory Map Management

```c
// src/kernel/memory/mmap_layout.c

// Memory map regions
#define MMAP_MIN_ADDR       0x10000000   // Minimum mmap address
#define MMAP_BASE           0xB0000000   // Default mmap base
#define STACK_TOP           0xC0000000   // Stack top
#define STACK_GAP           0x10000000   // Gap between stack and mmap

// Initialize mmap layout for process
void setup_mmap_layout(mm_struct_t *mm) {
    // Traditional layout
    mm->start_mmap = MMAP_BASE;
    
    // Reserve space for shared libraries
    create_library_mappings(mm);
}

// Create standard library mappings
static void create_library_mappings(mm_struct_t *mm) {
    // Reserve regions for common libraries
    // These are just reservations - actual mapping happens on demand
    
    // Dynamic linker
    reserve_vma_region(mm, 0x40000000, 0x40020000, "ld.so");
    
    // C library  
    reserve_vma_region(mm, 0x40100000, 0x40300000, "libc.so");
    
    // Thread library
    reserve_vma_region(mm, 0x40400000, 0x40500000, "libpthread.so");
}

// Find suitable address for mmap
uint32_t get_unmapped_area(mm_struct_t *mm, uint32_t addr, 
                          size_t len, uint32_t flags) {
    // Sanity checks
    if (len > TASK_SIZE || len == 0)
        return -ENOMEM;
    
    // Fixed address request
    if (flags & MAP_FIXED) {
        if (addr & ~PAGE_MASK)
            return -EINVAL;
        if (addr + len > TASK_SIZE)
            return -ENOMEM;
        
        // Check if region is free
        if (!check_region_free(mm, addr, len))
            return -EBUSY;
            
        return addr;
    }
    
    // Find free region
    if (addr) {
        addr = PAGE_ALIGN(addr);
        if (addr >= MMAP_MIN_ADDR && addr + len <= mm->start_mmap) {
            if (check_region_free(mm, addr, len))
                return addr;
        }
    }
    
    // Search for free space
    return find_vma_gap(mm, len, mm->start_mmap);
}

// Find gap between VMAs
static uint32_t find_vma_gap(mm_struct_t *mm, size_t len, uint32_t hint) {
    vma_t *vma;
    uint32_t addr = hint;
    
    // Start from hint address
    addr = MAX(addr, MMAP_MIN_ADDR);
    
    // Search through VMA list
    vma = mm->vma_list;
    while (vma) {
        if (addr + len <= vma->start) {
            // Found suitable gap
            return addr;
        }
        
        addr = vma->end;
        vma = vma->next;
    }
    
    // Check space after last VMA
    if (addr + len <= mm->start_mmap) {
        return addr;
    }
    
    return 0;  // No space found
}
```

## Thread Support

```c
// src/kernel/process/thread_layout.c

#define THREAD_STACK_SIZE   (2 * 1024 * 1024)  // 2MB per thread
#define MAX_THREADS         64                   // Max threads per process

// Thread info in process
typedef struct thread_info {
    tid_t tid;                    // Thread ID
    uint32_t stack_base;          // Stack base address
    uint32_t stack_size;          // Stack size
    void *tls_base;               // Thread-local storage
    
    // CPU state
    registers_t regs;             // Saved registers
    uint32_t kernel_stack;        // Kernel stack pointer
    
    struct thread_info *next;     // Next thread
} thread_info_t;

// Create new thread
int create_thread(process_t *proc, void (*entry)(void*), void *arg,
                 void *stack_addr, size_t stack_size) {
    thread_info_t *thread = kmalloc(sizeof(thread_info_t));
    if (!thread)
        return -ENOMEM;
    
    // Allocate thread ID
    thread->tid = allocate_tid();
    
    // Set up thread stack
    if (!stack_addr) {
        // Allocate new stack
        stack_size = stack_size ? stack_size : THREAD_STACK_SIZE;
        stack_addr = allocate_thread_stack(proc->mm, stack_size);
        if (!stack_addr) {
            kfree(thread);
            return -ENOMEM;
        }
    }
    
    thread->stack_base = (uint32_t)stack_addr - stack_size;
    thread->stack_size = stack_size;
    
    // Set up initial thread state
    memset(&thread->regs, 0, sizeof(registers_t));
    thread->regs.eip = (uint32_t)entry;
    thread->regs.esp = (uint32_t)stack_addr - 16;  // Initial stack
    thread->regs.eflags = 0x200;  // Enable interrupts
    
    // Push argument onto stack
    uint32_t *stack = (uint32_t*)(thread->regs.esp);
    stack[0] = 0;  // Fake return address
    stack[1] = (uint32_t)arg;
    
    // Allocate kernel stack
    thread->kernel_stack = (uint32_t)allocate_kernel_stack();
    
    // Add to process thread list
    thread->next = proc->threads;
    proc->threads = thread;
    proc->thread_count++;
    
    return thread->tid;
}

// Allocate stack for thread
static void* allocate_thread_stack(mm_struct_t *mm, size_t size) {
    // Find space in thread stack area
    uint32_t thread_stack_base = mm->start_stack - STACK_GAP;
    uint32_t addr = find_thread_stack_space(mm, size, thread_stack_base);
    
    if (!addr)
        return NULL;
    
    // Create VMA for thread stack
    vma_t *vma = create_vma(addr - size, addr,
                           PROT_READ | PROT_WRITE,
                           VMA_STACK | VMA_GROWSDOWN);
    if (!vma)
        return NULL;
    
    insert_vma(mm, vma);
    
    // Map initial pages
    for (int i = 0; i < 4; i++) {  // 16KB initial
        uint32_t page_addr = addr - (i + 1) * PAGE_SIZE;
        uint32_t frame = pmm_alloc_user_page();
        if (!frame) {
            remove_vma(mm, vma);
            return NULL;
        }
        
        vmm_map_page(mm->pgdir, page_addr, frame,
                    PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }
    
    return (void*)addr;
}
```

## Memory Layout Debugging

```c
// src/kernel/debug/memory_debug.c

// Print process memory map
void print_memory_map(process_t *proc) {
    mm_struct_t *mm = proc->mm;
    vma_t *vma;
    
    kprintf("Memory map for process %d (%s):\n", proc->pid, proc->name);
    kprintf("Start       End         Prot  Type      Description\n");
    kprintf("----------  ----------  ----  --------  -----------\n");
    
    for (vma = mm->vma_list; vma; vma = vma->next) {
        char prot[5] = "----";
        if (vma->prot & PROT_READ)  prot[0] = 'r';
        if (vma->prot & PROT_WRITE) prot[1] = 'w';
        if (vma->prot & PROT_EXEC)  prot[2] = 'x';
        if (vma->flags & VMA_SHARED) prot[3] = 's';
        else prot[3] = 'p';
        
        const char *type = get_vma_type_string(vma);
        const char *desc = get_vma_description(vma);
        
        kprintf("0x%08x  0x%08x  %s  %-8s  %s\n",
               vma->start, vma->end, prot, type, desc);
    }
    
    kprintf("\nMemory usage:\n");
    kprintf("  Code:   0x%08x - 0x%08x (%d KB)\n", 
           mm->start_code, mm->end_code,
           (mm->end_code - mm->start_code) / 1024);
    kprintf("  Data:   0x%08x - 0x%08x (%d KB)\n",
           mm->start_data, mm->end_data,
           (mm->end_data - mm->start_data) / 1024);
    kprintf("  Heap:   0x%08x - 0x%08x (%d KB)\n",
           mm->start_brk, mm->brk,
           (mm->brk - mm->start_brk) / 1024);
    kprintf("  Stack:  0x%08x (grows down)\n", mm->start_stack);
    kprintf("  Total:  %d KB (RSS: %d KB)\n",
           mm->total_vm * 4, mm->rss * 4);
}

// Get VMA type string
static const char* get_vma_type_string(vma_t *vma) {
    if (vma->flags & VMA_HEAP)  return "heap";
    if (vma->flags & VMA_STACK) return "stack";
    if (vma->flags & VMA_MMAP)  return "mmap";
    if (vma->prot & PROT_EXEC)  return "code";
    if (vma->prot & PROT_WRITE) return "data";
    return "other";
}

// /proc/[pid]/maps format output
void proc_maps_output(process_t *proc, char *buffer, size_t size) {
    mm_struct_t *mm = proc->mm;
    vma_t *vma;
    size_t len = 0;
    
    for (vma = mm->vma_list; vma && len < size; vma = vma->next) {
        char prot[5] = "----";
        if (vma->prot & PROT_READ)  prot[0] = 'r';
        if (vma->prot & PROT_WRITE) prot[1] = 'w';
        if (vma->prot & PROT_EXEC)  prot[2] = 'x';
        if (vma->flags & VMA_SHARED) prot[3] = 's';
        else prot[3] = 'p';
        
        if (vma->file) {
            len += snprintf(buffer + len, size - len,
                          "%08x-%08x %s %08x %02x:%02x %d %s\n",
                          vma->start, vma->end, prot,
                          vma->file_offset,
                          MAJOR(vma->file->device),
                          MINOR(vma->file->device),
                          vma->file->inode,
                          vma->file->path);
        } else {
            const char *desc = "";
            if (vma->flags & VMA_HEAP) desc = "[heap]";
            else if (vma->flags & VMA_STACK) {
                if (vma == find_main_stack(mm))
                    desc = "[stack]";
                else
                    desc = "[thread stack]";
            }
            
            len += snprintf(buffer + len, size - len,
                          "%08x-%08x %s 00000000 00:00 0 %s\n",
                          vma->start, vma->end, prot, desc);
        }
    }
}
```

## Testing Process Memory Layout

```c
// tests/process_layout_test.c

void test_elf_loading(void) {
    // Test loading simple ELF executable
    process_t *proc = create_process();
    
    int result = load_elf("/bin/test_program", proc);
    assert(result == 0);
    
    // Verify memory layout
    assert(proc->mm->start_code == 0x08048000);
    assert(proc->mm->end_code > proc->mm->start_code);
    assert(proc->mm->start_data > proc->mm->end_code);
    assert(proc->mm->end_data > proc->mm->start_data);
    assert(proc->mm->start_brk == PAGE_ALIGN(proc->mm->end_data));
    assert(proc->mm->start_stack < USER_SPACE_END);
    
    // Verify page mappings
    assert(is_page_mapped(proc->mm, proc->mm->start_code));
    assert(is_page_mapped(proc->mm, proc->mm->start_stack - PAGE_SIZE));
    
    destroy_process(proc);
}

void test_stack_setup(void) {
    // Test stack setup with arguments
    process_t *proc = create_process();
    char *argv[] = {"test", "arg1", "arg2", NULL};
    char *envp[] = {"PATH=/bin", "HOME=/home/user", NULL};
    
    setup_user_stack(proc->mm, proc, 3, argv, envp);
    
    // Verify stack contents
    uint32_t sp = proc->mm->start_stack;
    int argc;
    copy_from_user(&argc, (void*)sp, sizeof(int));
    assert(argc == 3);
    
    // Verify environment accessible
    sp += sizeof(int);
    char **argv_ptr;
    copy_from_user(&argv_ptr, (void*)sp, sizeof(char**));
    assert(argv_ptr != NULL);
    
    destroy_process(proc);
}

void test_memory_limits(void) {
    // Test memory limit enforcement
    process_t *proc = create_process();
    
    // Set heap limit
    proc->rlim[RLIMIT_DATA].rlim_cur = 1024 * 1024;  // 1MB
    
    // Try to allocate beyond limit
    void *result = sys_brk((void*)(proc->mm->start_brk + 2 * 1024 * 1024));
    assert(result == (void*)proc->mm->brk);  // Should fail
    
    // Allocate within limit
    result = sys_brk((void*)(proc->mm->start_brk + 512 * 1024));
    assert(result != (void*)proc->mm->brk);  // Should succeed
    
    destroy_process(proc);
}
```

## Performance Optimizations

### Layout Optimizations

1. **Cache-Friendly Layout**: Align hot data structures to cache lines
2. **TLB Optimization**: Use huge pages for large mappings
3. **NUMA Awareness**: Allocate memory close to executing CPU
4. **Prefaulting**: Optionally prefault critical pages

### Security Enhancements

1. **ASLR Preparation**: Randomizable base addresses
2. **Stack Canaries**: Detect stack buffer overflows
3. **W^X Enforcement**: No pages both writable and executable
4. **Guard Pages**: Detect overflows and underflows