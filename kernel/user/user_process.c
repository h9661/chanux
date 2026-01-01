/**
 * =============================================================================
 * Chanux OS - User Process Management
 * =============================================================================
 * Implements user-mode process creation and management:
 *   - User address space creation
 *   - User stack allocation
 *   - User code loading
 *   - Entry to user mode via IRETQ
 * =============================================================================
 */

#include "user/user.h"
#include "proc/process.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "kernel.h"
#include "gdt.h"
#include "drivers/vga/vga.h"
#include "string.h"
#include "debug.h"

/* =============================================================================
 * User Stack Allocation
 * =============================================================================
 */

/**
 * Allocate and map a user stack for a process.
 *
 * @param proc Process to allocate stack for
 * @return true on success, false on failure
 */
bool user_stack_alloc(process_t* proc) {
    if (!proc || proc->pml4_phys == 0) {
        return false;
    }

    /* Calculate stack base (grows down from USER_STACK_TOP) */
    virt_addr_t stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    uint32_t pages = USER_STACK_SIZE / PAGE_SIZE;

    /* Allocate and map each page */
    for (uint32_t i = 0; i < pages; i++) {
        phys_addr_t page = pmm_alloc_page();
        if (page == 0) {
            /* Allocation failed - unmap what we've done */
            for (uint32_t j = 0; j < i; j++) {
                /* TODO: Properly free pages - need vmm_get_physical for user space */
            }
            return false;
        }

        /* Zero the page */
        memset(PHYS_TO_VIRT(page), 0, PAGE_SIZE);

        /* Map with user RW, no execute */
        if (!vmm_map_user_page(proc->pml4_phys,
                               stack_base + i * PAGE_SIZE,
                               page,
                               PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NX)) {
            pmm_free_page(page);
            return false;
        }
    }

    /* Set up stack pointer (points to top of stack, 16-byte aligned) */
    proc->user_stack = (void*)stack_base;
    proc->user_stack_top = (USER_STACK_TOP - 16) & ~0xFULL;  /* 16-byte aligned */

    return true;
}

/**
 * Free a user stack.
 *
 * @param proc Process whose stack to free
 */
void user_stack_free(process_t* proc) {
    if (!proc || proc->pml4_phys == 0 || proc->user_stack == 0) {
        return;
    }

    virt_addr_t stack_base = (virt_addr_t)proc->user_stack;
    uint32_t pages = USER_STACK_SIZE / PAGE_SIZE;

    /* We need to switch to the process's address space to unmap */
    phys_addr_t current_cr3 = read_cr3();
    vmm_switch_address_space(proc->pml4_phys);

    /* Free each page */
    for (uint32_t i = 0; i < pages; i++) {
        virt_addr_t addr = stack_base + i * PAGE_SIZE;
        phys_addr_t phys = vmm_get_physical(addr);
        if (phys != 0) {
            pmm_free_page(phys);
        }
    }

    /* Switch back */
    vmm_switch_address_space(current_cr3);

    proc->user_stack = NULL;
    proc->user_stack_top = 0;
}

/* =============================================================================
 * User Code Loading
 * =============================================================================
 */

/**
 * Load user code into a process's address space.
 *
 * @param proc      Process to load code into
 * @param code      Pointer to code data (in kernel space)
 * @param code_size Size of code in bytes
 * @param entry     Virtual address to load code at
 * @return true on success, false on failure
 */
bool user_load_code(process_t* proc, const void* code, size_t code_size,
                    virt_addr_t entry) {
    DBG_USER(">>> user_load_code START <<<\n");

    if (!proc || !code || code_size == 0 || proc->pml4_phys == 0) {
        kprintf("user_load_code: validation failed\n");
        return false;
    }

    /* Ensure entry is in user space and page-aligned */
    if (entry >= USER_SPACE_END || entry < USER_SPACE_START) {
        kprintf("user_load_code: entry out of range\n");
        return false;
    }

    virt_addr_t code_base = ALIGN_DOWN(entry, PAGE_SIZE);
    size_t aligned_size = ALIGN_UP(code_size + (entry - code_base), PAGE_SIZE);
    uint32_t pages = aligned_size / PAGE_SIZE;
    DBG_USER("user_load_code: pages=%u, code_base=0x%llx\n",
            pages, (unsigned long long)code_base);

    const uint8_t* src = (const uint8_t*)code;
    size_t bytes_copied = 0;
    size_t offset = entry - code_base;  /* Offset for first page */

    /* Allocate, map, and copy code - all in one pass */
    for (uint32_t i = 0; i < pages; i++) {
        phys_addr_t page = pmm_alloc_page();
        if (page == 0) {
            return false;
        }

        /* Get kernel virtual address for this physical page */
        uint8_t* dst = (uint8_t*)PHYS_TO_VIRT(page);

        /* Zero the page first */
        memset(dst, 0, PAGE_SIZE);

        /* Copy code to this page (use physical address via kernel mapping) */
        if (bytes_copied < code_size) {
            size_t page_offset = (i == 0) ? offset : 0;
            size_t to_copy = PAGE_SIZE - page_offset;
            if (to_copy > code_size - bytes_copied) {
                to_copy = code_size - bytes_copied;
            }
            memcpy(dst + page_offset, src + bytes_copied, to_copy);
            bytes_copied += to_copy;
        }

        /* Map with user RO (code should be read-only) */
        DBG_USER("user_load_code: mapping page %d at 0x%llx (pml4=0x%x)\n",
                i, code_base + i * PAGE_SIZE, (uint32_t)proc->pml4_phys);
        if (!vmm_map_user_page(proc->pml4_phys,
                               code_base + i * PAGE_SIZE,
                               page,
                               PTE_PRESENT | PTE_USER)) {
            kprintf("user_load_code: vmm_map_user_page FAILED!\n");
            pmm_free_page(page);
            return false;
        }
        DBG_USER("user_load_code: page mapped successfully\n");
    }

    proc->user_code = (void*)entry;
    proc->user_code_size = code_size;

    return true;
}

/* =============================================================================
 * User Process Entry Point
 * =============================================================================
 */

/**
 * Wrapper function that serves as the kernel entry point for user processes.
 * When this process is scheduled, it switches to user's address space and
 * enters user mode via IRETQ.
 */
static void user_process_entry(void* arg) {
    (void)arg;  /* Unused */

    process_t* proc = process_current();

    DBG_USER("user: Process %d entering user mode\n", proc->pid);
    DBG_USER("user: CR3=0x%x, entry=0x%llx, stack=0x%llx\n",
            (uint32_t)proc->pml4_phys,
            (unsigned long long)(uint64_t)proc->user_code,
            (unsigned long long)proc->user_stack_top);

    /* Switch to user's address space */
    vmm_switch_address_space(proc->pml4_phys);

    /* Enter user mode (never returns) */
    user_mode_enter((uint64_t)proc->user_code, proc->user_stack_top);

    /* Should never reach here */
    kprintf("user: ERROR - user_mode_enter returned!\n");
    for (;;) { __asm__ volatile("hlt"); }
}

/* =============================================================================
 * User Process Creation
 * =============================================================================
 */

/**
 * Create a user-mode process.
 *
 * @param name      Process name
 * @param code      Pointer to user code (binary)
 * @param code_size Size of user code
 * @return PID on success, (pid_t)-1 on failure
 */
pid_t user_process_create(const char* name, const void* code, size_t code_size) {
    /*
     * Create a user-mode process:
     * 1. Create address space (PML4)
     * 2. Load user code into address space
     * 3. Allocate user stack
     * 4. Create kernel process with user mode flag
     * 5. Set up initial user RSP and RIP
     */

    DBG_USER("user: Creating user process '%s' (%u bytes)\n", name, (uint32_t)code_size);

    /* Create address space */
    phys_addr_t pml4 = vmm_create_address_space();
    if (pml4 == 0) {
        kprintf("user: Failed to create address space\n");
        return (pid_t)-1;
    }

    DBG_USER("user: Created address space at 0x%x\n", (uint32_t)pml4);

    /* Create a temporary process structure for loading */
    process_t temp_proc = {0};
    temp_proc.pml4_phys = pml4;

    /* Load user code at USER_CODE_BASE */
    DBG_USER("user: About to call user_load_code\n");
    bool load_result = user_load_code(&temp_proc, code, code_size, USER_CODE_BASE);
    DBG_USER("user: user_load_code returned %d\n", load_result);
    if (!load_result) {
        kprintf("user: Failed to load user code\n");
        vmm_destroy_address_space(pml4);
        return (pid_t)-1;
    }

    DBG_USER("user: Loaded code at 0x%x\n", (uint32_t)USER_CODE_BASE);

    /* Allocate user stack */
    if (!user_stack_alloc(&temp_proc)) {
        kprintf("user: Failed to allocate user stack\n");
        vmm_destroy_address_space(pml4);
        return (pid_t)-1;
    }

    DBG_USER("user: Allocated user stack, RSP=0x%llx\n",
            (unsigned long long)temp_proc.user_stack_top);

    /*
     * Create the kernel process with user mode wrapper.
     * When scheduled, user_process_entry() will switch CR3 and enter Ring 3.
     */
    pid_t pid = process_create(name, user_process_entry, NULL);
    if (pid == (pid_t)-1) {
        kprintf("user: Failed to create kernel process\n");
        vmm_destroy_address_space(pml4);
        return (pid_t)-1;
    }

    /* Get the process and set user mode fields */
    process_t* proc = process_get(pid);
    if (!proc) {
        kprintf("user: Failed to get process %d\n", pid);
        vmm_destroy_address_space(pml4);
        return (pid_t)-1;
    }

    proc->flags |= PROCESS_FLAG_USER;
    proc->pml4_phys = pml4;
    proc->user_stack = temp_proc.user_stack;
    proc->user_stack_top = temp_proc.user_stack_top;
    proc->user_code = temp_proc.user_code;
    proc->user_code_size = temp_proc.user_code_size;

    DBG_USER("user: Created user process '%s' with PID %d\n", name, pid);

    return pid;
}
