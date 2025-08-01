# Memory Protection Mechanisms Specification

## Overview

This document specifies comprehensive memory protection mechanisms for ChanUX to ensure secure user mode operation, including access control, stack protection, execution prevention, and advanced security features.

## Protection Levels and Rings

### x86 Protection Rings

```
Ring 0 (Kernel Mode)
├── Full hardware access
├── All instructions available
├── Access to all memory
└── Interrupt/exception handling

Ring 3 (User Mode)
├── Restricted hardware access
├── Limited instruction set
├── Access only to user pages
└── System calls for privileged operations
```

### Page-Level Protection

```c
// src/kernel/memory/protection.h

// Page protection flags
#define PAGE_PRESENT    0x001    // Page is present in memory
#define PAGE_RW         0x002    // Page is writable
#define PAGE_USER       0x004    // Page is user accessible
#define PAGE_PWT        0x008    // Page write-through
#define PAGE_PCD        0x010    // Page cache disabled
#define PAGE_ACCESSED   0x020    // Page has been accessed
#define PAGE_DIRTY      0x040    // Page has been written to
#define PAGE_PSE        0x080    // 4MB page (Page Size Extension)
#define PAGE_GLOBAL     0x100    // Global page (not flushed from TLB)

// Software-defined flags (available bits)
#define PAGE_COW        0x200    // Copy-on-write page
#define PAGE_SHARED     0x400    // Shared between processes
#define PAGE_LOCKED     0x800    // Page locked in memory

// NX bit support (if available)
#define PAGE_NX         (1ULL << 63)  // No-execute bit

// Protection combinations
#define PAGE_KERNEL_RO  (PAGE_PRESENT)
#define PAGE_KERNEL_RW  (PAGE_PRESENT | PAGE_RW)
#define PAGE_USER_RO    (PAGE_PRESENT | PAGE_USER)
#define PAGE_USER_RW    (PAGE_PRESENT | PAGE_RW | PAGE_USER)
#define PAGE_USER_EXEC  (PAGE_PRESENT | PAGE_USER)  // Without NX
#define PAGE_USER_DATA  (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_NX)
```

## Access Control Implementation

### CPU Feature Detection

```c
// src/kernel/cpu/features.c

typedef struct {
    bool nx_bit;          // No-execute bit support
    bool pae;             // Physical Address Extension
    bool pge;             // Page Global Enable
    bool pat;             // Page Attribute Table
    bool smep;            // Supervisor Mode Execution Prevention
    bool smap;            // Supervisor Mode Access Prevention
    bool umip;            // User Mode Instruction Prevention
} cpu_features_t;

static cpu_features_t cpu_features;

// Initialize CPU security features
void init_cpu_security_features(void) {
    uint32_t eax, ebx, ecx, edx;
    
    // Check for NX bit support
    cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
    cpu_features.nx_bit = (edx & (1 << 20)) != 0;
    
    // Check for PAE (required for NX)
    cpuid(1, &eax, &ebx, &ecx, &edx);
    cpu_features.pae = (edx & (1 << 6)) != 0;
    cpu_features.pge = (edx & (1 << 13)) != 0;
    
    // Check extended features
    cpuid_count(7, 0, &eax, &ebx, &ecx, &edx);
    cpu_features.smep = (ebx & (1 << 7)) != 0;
    cpu_features.smap = (ebx & (1 << 20)) != 0;
    cpu_features.umip = (ecx & (1 << 2)) != 0;
    
    // Enable security features
    if (cpu_features.nx_bit && cpu_features.pae) {
        enable_nx_bit();
    }
    
    if (cpu_features.smep) {
        enable_smep();
    }
    
    if (cpu_features.smap) {
        enable_smap();
    }
}

// Enable NX bit
static void enable_nx_bit(void) {
    // Enable PAE first (required for NX)
    uint32_t cr4 = read_cr4();
    cr4 |= CR4_PAE;
    write_cr4(cr4);
    
    // Enable NX in EFER MSR
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_NXE;
    wrmsr(MSR_EFER, efer);
    
    kprintf("NX bit protection enabled\n");
}

// Enable SMEP (Supervisor Mode Execution Prevention)
static void enable_smep(void) {
    uint32_t cr4 = read_cr4();
    cr4 |= CR4_SMEP;
    write_cr4(cr4);
    
    kprintf("SMEP protection enabled\n");
}

// Enable SMAP (Supervisor Mode Access Prevention)
static void enable_smap(void) {
    uint32_t cr4 = read_cr4();
    cr4 |= CR4_SMAP;
    write_cr4(cr4);
    
    kprintf("SMAP protection enabled\n");
}
```

### Enhanced Page Fault Handler

```c
// src/kernel/memory/fault_handler.c

// Enhanced page fault handler with security checks
void page_fault_handler(registers_t *regs) {
    uint32_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r" (fault_addr));
    
    // Decode error code
    fault_error_t error = decode_fault_error(regs->err_code);
    
    // Security checks
    if (perform_security_checks(fault_addr, error, regs) == SECURITY_VIOLATION) {
        handle_security_violation(current_process, fault_addr, error);
        return;
    }
    
    // Handle legitimate faults
    if (error.user_mode) {
        if (!handle_user_fault(fault_addr, error)) {
            // Send SIGSEGV to process
            send_signal(current_process, SIGSEGV);
        }
    } else {
        if (!handle_kernel_fault(fault_addr, error)) {
            // Kernel fault - this is serious
            panic("Kernel page fault at 0x%x, IP=0x%x", 
                  fault_addr, regs->eip);
        }
    }
}

// Decode page fault error code
static fault_error_t decode_fault_error(uint32_t err_code) {
    return (fault_error_t) {
        .present = (err_code & 0x1) != 0,
        .write = (err_code & 0x2) != 0,
        .user_mode = (err_code & 0x4) != 0,
        .reserved_write = (err_code & 0x8) != 0,
        .instruction_fetch = (err_code & 0x10) != 0,
        .protection_key = (err_code & 0x20) != 0,
        .shadow_stack = (err_code & 0x40) != 0,
    };
}

// Perform security checks
static security_result_t perform_security_checks(uint32_t addr, 
                                                fault_error_t error,
                                                registers_t *regs) {
    // Check 1: User mode access to kernel space
    if (error.user_mode && addr >= KERNEL_SPACE_START) {
        kprintf("Security: User access to kernel address 0x%x\n", addr);
        return SECURITY_VIOLATION;
    }
    
    // Check 2: Kernel access to user space without SMAP disabled
    if (!error.user_mode && addr < KERNEL_SPACE_START) {
        if (cpu_features.smap && !(regs->eflags & EFLAGS_AC)) {
            kprintf("Security: SMAP violation at 0x%x\n", addr);
            return SECURITY_VIOLATION;
        }
    }
    
    // Check 3: Execute from non-executable page
    if (error.instruction_fetch) {
        page_table_entry_t *pte = get_page_entry(current_page_dir, addr);
        if (pte && pte->present && (pte->flags & PAGE_NX)) {
            kprintf("Security: NX violation at 0x%x\n", addr);
            return SECURITY_VIOLATION;
        }
    }
    
    // Check 4: Stack overflow detection
    if (error.user_mode && is_stack_overflow(current_process, addr)) {
        kprintf("Security: Stack overflow detected at 0x%x\n", addr);
        return SECURITY_VIOLATION;
    }
    
    return SECURITY_OK;
}
```

## Stack Protection

### Stack Canaries

```c
// src/kernel/security/stack_canary.c

// Stack canary value (randomized at boot)
static uint32_t stack_canary_value __attribute__((section(".data.canary")));

// Initialize stack canaries
void init_stack_canaries(void) {
    // Generate random canary value
    stack_canary_value = generate_random_u32();
    
    // Ensure canary has NULL byte to stop string operations
    stack_canary_value &= 0xFFFFFF00;
    stack_canary_value |= 0x00;
    
    // Set up GS segment for fast canary access
    setup_canary_segment();
}

// Function prologue with canary
void __stack_chk_guard(void) {
    asm volatile(
        "mov %%gs:0x14, %%eax\n"
        "mov %%eax, -0x4(%%ebp)\n"
        ::: "eax"
    );
}

// Function epilogue with canary check
void __stack_chk_fail(void) {
    panic("Stack smashing detected!");
}

// Compiler-generated canary check
__attribute__((naked)) void __stack_chk_check(void) {
    asm volatile(
        "mov -0x4(%%ebp), %%eax\n"
        "xor %%gs:0x14, %%eax\n"
        "jnz __stack_chk_fail\n"
        "ret\n"
    );
}
```

### Guard Pages

```c
// src/kernel/memory/guard_pages.c

// Create guard page below stack
int create_stack_guard_page(mm_struct_t *mm, uint32_t stack_bottom) {
    // Guard page is not mapped - access causes fault
    vma_t *guard_vma = create_vma(stack_bottom - PAGE_SIZE, stack_bottom,
                                 0,  // No permissions
                                 VMA_GUARD);
    if (!guard_vma)
        return -ENOMEM;
    
    insert_vma(mm, guard_vma);
    
    // Ensure page is not mapped
    page_table_entry_t *pte = get_page_entry(mm->pgdir, stack_bottom - PAGE_SIZE);
    if (pte) {
        pte->present = 0;
        tlb_invalidate_page(stack_bottom - PAGE_SIZE);
    }
    
    return 0;
}

// Check for stack overflow
bool is_stack_overflow(process_t *proc, uint32_t fault_addr) {
    vma_t *stack_vma = find_stack_vma(proc->mm);
    if (!stack_vma)
        return false;
    
    // Check if fault is in guard page area
    uint32_t guard_start = stack_vma->start - PAGE_SIZE;
    uint32_t guard_end = stack_vma->start;
    
    return fault_addr >= guard_start && fault_addr < guard_end;
}

// Handle stack expansion
bool handle_stack_expansion(mm_struct_t *mm, uint32_t fault_addr) {
    vma_t *stack_vma = find_stack_vma(mm);
    if (!stack_vma)
        return false;
    
    // Check if expansion is reasonable
    uint32_t expansion_size = stack_vma->start - (fault_addr & PAGE_MASK);
    if (expansion_size > MAX_STACK_EXPANSION)
        return false;
    
    // Check against stack limit
    struct rlimit *rlim = &current_process->rlim[RLIMIT_STACK];
    if (stack_vma->end - (fault_addr & PAGE_MASK) > rlim->rlim_cur)
        return false;
    
    // Expand stack VMA
    uint32_t new_start = fault_addr & PAGE_MASK;
    
    // Allocate pages for expansion
    for (uint32_t addr = new_start; addr < stack_vma->start; addr += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_user_page();
        if (!frame) {
            // Rollback
            for (uint32_t rollback = new_start; rollback < addr; rollback += PAGE_SIZE) {
                page_table_entry_t *pte = get_page_entry(mm->pgdir, rollback);
                if (pte && pte->present) {
                    pmm_free_page(pte->frame << 12);
                    pte->present = 0;
                }
            }
            return false;
        }
        
        vmm_map_page(mm->pgdir, addr, frame, PAGE_USER_RW);
    }
    
    // Update VMA
    stack_vma->start = new_start;
    
    // Move guard page
    create_stack_guard_page(mm, new_start);
    
    return true;
}
```

## W^X (Write XOR Execute) Protection

```c
// src/kernel/memory/nx_protection.c

// Enforce W^X protection on VMA
void enforce_wx_protection(vma_t *vma) {
    if (!cpu_features.nx_bit)
        return;  // NX not supported
    
    // Cannot be both writable and executable
    if ((vma->prot & PROT_WRITE) && (vma->prot & PROT_EXEC)) {
        kprintf("Warning: W^X violation in VMA 0x%x-0x%x\n", 
                vma->start, vma->end);
        
        // Force choose based on usage
        if (vma->type == VMA_CODE) {
            // Code sections should be executable, not writable
            vma->prot &= ~PROT_WRITE;
        } else {
            // Data sections should be writable, not executable
            vma->prot &= ~PROT_EXEC;
        }
    }
    
    // Update page table entries
    update_vma_protection(vma);
}

// Update page protection for VMA
static void update_vma_protection(vma_t *vma) {
    for (uint32_t addr = vma->start; addr < vma->end; addr += PAGE_SIZE) {
        page_table_entry_t *pte = get_page_entry(current_page_dir, addr);
        if (pte && pte->present) {
            // Update protection flags
            uint64_t flags = pte->flags;
            
            // Clear and set protection bits
            flags &= ~(PAGE_RW | PAGE_NX);
            
            if (vma->prot & PROT_WRITE)
                flags |= PAGE_RW;
            
            if (!(vma->prot & PROT_EXEC))
                flags |= PAGE_NX;
            
            pte->flags = flags;
            tlb_invalidate_page(addr);
        }
    }
}

// JIT code support - temporary W+X permission
int create_jit_region(mm_struct_t *mm, size_t size, void **addr) {
    // Allocate two mappings for same physical pages
    // One for writing (W), one for executing (X)
    
    uint32_t write_addr = find_free_region(mm, size * 2);
    if (!write_addr)
        return -ENOMEM;
    
    uint32_t exec_addr = write_addr + size;
    
    // Create write mapping
    vma_t *write_vma = create_vma(write_addr, write_addr + size,
                                 PROT_READ | PROT_WRITE,
                                 VMA_JIT | VMA_WRITE);
    
    // Create execute mapping  
    vma_t *exec_vma = create_vma(exec_addr, exec_addr + size,
                                PROT_READ | PROT_EXEC,
                                VMA_JIT | VMA_EXEC);
    
    // Allocate physical pages
    for (size_t i = 0; i < size; i += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_user_page();
        if (!frame) {
            // Cleanup
            return -ENOMEM;
        }
        
        // Map to both addresses
        vmm_map_page(mm->pgdir, write_addr + i, frame, PAGE_USER_RW);
        vmm_map_page(mm->pgdir, exec_addr + i, frame, PAGE_USER_EXEC);
    }
    
    insert_vma(mm, write_vma);
    insert_vma(mm, exec_vma);
    
    *addr = (void*)write_addr;
    return 0;
}
```

## Copy-on-Write (COW) Protection

```c
// src/kernel/memory/cow.c

// Mark pages as copy-on-write
void mark_pages_cow(mm_struct_t *mm, uint32_t start, uint32_t end) {
    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        page_table_entry_t *pte = get_page_entry(mm->pgdir, addr);
        if (pte && pte->present && pte->rw) {
            // Mark as read-only and COW
            pte->rw = 0;
            pte->cow = 1;
            
            // Increase reference count
            pmm_ref_page(pte->frame << 12);
            
            tlb_invalidate_page(addr);
        }
    }
}

// Handle COW fault
bool handle_cow_fault(mm_struct_t *mm, uint32_t fault_addr) {
    page_table_entry_t *pte = get_page_entry(mm->pgdir, fault_addr);
    
    if (!pte || !pte->present || !pte->cow)
        return false;
    
    uint32_t old_frame = pte->frame << 12;
    uint32_t ref_count = pmm_get_ref_count(old_frame);
    
    if (ref_count == 1) {
        // We're the only user - just make it writable
        pte->rw = 1;
        pte->cow = 0;
        tlb_invalidate_page(fault_addr);
        return true;
    }
    
    // Allocate new page
    uint32_t new_frame = pmm_alloc_user_page();
    if (!new_frame)
        return false;
    
    // Copy page contents
    memcpy((void*)new_frame, (void*)old_frame, PAGE_SIZE);
    
    // Update page table
    pte->frame = new_frame >> 12;
    pte->rw = 1;
    pte->cow = 0;
    
    // Decrease reference count on old page
    pmm_unref_page(old_frame);
    
    tlb_invalidate_page(fault_addr);
    return true;
}

// Fork with COW
int fork_with_cow(process_t *parent, process_t *child) {
    mm_struct_t *parent_mm = parent->mm;
    mm_struct_t *child_mm = create_mm();
    
    if (!child_mm)
        return -ENOMEM;
    
    // Copy VMAs
    vma_t *vma;
    for (vma = parent_mm->vma_list; vma; vma = vma->next) {
        vma_t *new_vma = copy_vma(vma);
        if (!new_vma) {
            destroy_mm(child_mm);
            return -ENOMEM;
        }
        
        insert_vma(child_mm, new_vma);
        
        // Mark pages as COW in both parent and child
        if (vma->prot & PROT_WRITE) {
            mark_pages_cow(parent_mm, vma->start, vma->end);
            mark_pages_cow(child_mm, vma->start, vma->end);
        }
    }
    
    child->mm = child_mm;
    return 0;
}
```

## ASLR (Address Space Layout Randomization) Foundation

```c
// src/kernel/security/aslr.c

// ASLR configuration
typedef struct {
    bool enabled;
    uint32_t stack_random_bits;
    uint32_t mmap_random_bits;
    uint32_t heap_random_bits;
} aslr_config_t;

static aslr_config_t aslr_config = {
    .enabled = true,
    .stack_random_bits = 19,  // 512KB randomization
    .mmap_random_bits = 8,    // 1MB randomization  
    .heap_random_bits = 13,   // 32KB randomization
};

// Randomize stack placement
uint32_t randomize_stack_top(uint32_t stack_top) {
    if (!aslr_config.enabled)
        return stack_top;
    
    uint32_t random_offset = generate_random_u32() & 
                            ((1 << aslr_config.stack_random_bits) - 1);
    
    // Align to 16 bytes
    random_offset &= ~0xF;
    
    return stack_top - random_offset;
}

// Randomize mmap base
uint32_t randomize_mmap_base(uint32_t mmap_base) {
    if (!aslr_config.enabled)
        return mmap_base;
    
    uint32_t random_offset = generate_random_u32() & 
                            ((1 << aslr_config.mmap_random_bits) - 1);
    
    // Page align
    random_offset &= PAGE_MASK;
    
    return mmap_base - random_offset;
}

// Randomize heap start
uint32_t randomize_heap_start(uint32_t heap_start) {
    if (!aslr_config.enabled)
        return heap_start;
    
    uint32_t random_offset = generate_random_u32() & 
                            ((1 << aslr_config.heap_random_bits) - 1);
    
    // Page align
    random_offset &= PAGE_MASK;
    
    return heap_start + random_offset;
}

// Apply ASLR to process
void apply_aslr(process_t *proc) {
    mm_struct_t *mm = proc->mm;
    
    // Randomize stack
    mm->start_stack = randomize_stack_top(USER_STACK_TOP);
    
    // Randomize mmap region
    mm->start_mmap = randomize_mmap_base(MMAP_BASE);
    
    // Randomize heap (after loading executable)
    mm->start_brk = randomize_heap_start(mm->start_brk);
    mm->brk = mm->start_brk;
}
```

## Security Monitoring and Auditing

```c
// src/kernel/security/audit.c

// Security event types
typedef enum {
    SEC_EVENT_NX_VIOLATION,
    SEC_EVENT_SMAP_VIOLATION,
    SEC_EVENT_SMEP_VIOLATION,
    SEC_EVENT_STACK_OVERFLOW,
    SEC_EVENT_BUFFER_OVERFLOW,
    SEC_EVENT_INVALID_SYSCALL,
    SEC_EVENT_PRIVILEGE_ESCALATION,
} security_event_t;

// Security audit entry
typedef struct {
    timestamp_t timestamp;
    pid_t pid;
    security_event_t event;
    uint32_t address;
    uint32_t instruction_pointer;
    char details[64];
} security_audit_entry_t;

// Log security event
void log_security_event(security_event_t event, uint32_t addr, 
                       const char *fmt, ...) {
    security_audit_entry_t entry = {
        .timestamp = get_system_time(),
        .pid = current_process->pid,
        .event = event,
        .address = addr,
        .instruction_pointer = get_instruction_pointer(),
    };
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.details, sizeof(entry.details), fmt, args);
    va_end(args);
    
    // Add to audit log
    add_audit_entry(&entry);
    
    // Alert if critical
    if (is_critical_event(event)) {
        kprintf("SECURITY ALERT: PID %d - %s at 0x%x\n",
               entry.pid, get_event_name(event), addr);
    }
}

// Memory access tracking
void track_memory_access(uint32_t addr, access_type_t type) {
    if (!security_monitoring_enabled)
        return;
    
    // Check for suspicious patterns
    if (is_suspicious_access(addr, type)) {
        log_security_event(SEC_EVENT_SUSPICIOUS_ACCESS, addr,
                         "Suspicious %s access pattern detected",
                         type == ACCESS_READ ? "read" : "write");
    }
}
```

## Testing Memory Protection

```c
// tests/memory_protection_test.c

// Test NX bit enforcement
void test_nx_protection(void) {
    // Allocate data page
    void *data_page = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(data_page != MAP_FAILED);
    
    // Write shellcode
    uint8_t shellcode[] = {0xC3};  // ret instruction
    memcpy(data_page, shellcode, sizeof(shellcode));
    
    // Try to execute - should fail
    void (*func)(void) = data_page;
    
    // Set up signal handler for SIGSEGV
    signal(SIGSEGV, segv_handler);
    
    // This should trigger SIGSEGV
    func();
    
    // Should not reach here
    assert(false);
}

// Test stack protection
void test_stack_protection(void) {
    // Test stack canary
    char buffer[16];
    
    // Deliberate buffer overflow
    memset(buffer, 'A', 32);  // Overflow!
    
    // Should trigger stack smashing detection
    // and not reach here
    assert(false);
}

// Test COW mechanism
void test_cow_protection(void) {
    // Create shared memory
    int *shared = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *shared = 42;
    
    if (fork() == 0) {
        // Child - modify shared memory
        *shared = 24;
        exit(0);
    }
    
    // Parent - wait and check
    wait(NULL);
    assert(*shared == 24);  // Should see child's change
    
    // Now test private mapping
    int *private = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    *private = 42;
    
    if (fork() == 0) {
        // Child - modify private memory
        *private = 24;
        assert(*private == 24);
        exit(0);
    }
    
    // Parent - should not see child's change
    wait(NULL);
    assert(*private == 42);
}
```

## Performance Impact

### Protection Overhead

1. **NX bit checking**: ~5 cycles per page fault
2. **COW handling**: ~1000 cycles for page copy
3. **Stack canaries**: ~10 cycles per function
4. **ASLR**: One-time cost at process creation
5. **Access validation**: ~20 cycles per check

### Optimization Strategies

1. **TLB optimization**: Batch protection changes
2. **COW optimization**: Use huge pages when possible
3. **Canary optimization**: Only for security-critical functions
4. **Validation caching**: Cache validation results