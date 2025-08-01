/*
 * Virtual Memory Manager Protection Extensions
 * 
 * This file implements enhanced VMM functions with proper user/supervisor
 * bit enforcement and security checks for user mode support.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/vmm.h"
#include "kernel/pmm.h"
#include "kernel/memory_protection.h"
#include "kernel/process.h"
#include "lib/string.h"

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_write_hex(uint32_t value);
extern process_t* process_get_current(void);
extern void process_terminate(process_t* proc, int signal);

/* Wrapper for consistency - use process_get_current directly */
#define get_current_process() process_get_current()

/* Enhanced page mapping with security checks */
void vmm_map_page_secure(page_directory_t* page_dir, uint32_t virt_addr, 
                        uint32_t phys_addr, uint32_t flags) {
    /* Validate that kernel pages don't have USER flag */
    if (virt_addr < KERNEL_SPACE_END && (flags & PAGE_USER)) {
        terminal_writestring("VMM: Security violation - attempt to map kernel page as user accessible\n");
        return;
    }
    
    /* User pages must have USER flag */
    if (virt_addr >= USER_SPACE_START && !(flags & PAGE_USER)) {
        terminal_writestring("VMM: Warning - user space page mapped without USER flag\n");
        flags |= PAGE_USER;  /* Force USER flag for safety */
    }
    
    /* Use our enhanced mapping function that handles page table permissions correctly */
    extern void vmm_map_page_with_secure_table(page_directory_t* page_dir, uint32_t virt_addr, 
                                               uint32_t phys_addr, uint32_t flags);
    vmm_map_page_with_secure_table(page_dir, virt_addr, phys_addr, flags);
}

/* Get page table entry with proper checks */
page_table_entry_t* vmm_get_page_entry(page_directory_t* page_dir, uint32_t virt_addr) {
    uint32_t dir_index = PAGE_DIR_INDEX(virt_addr);
    
    /* Check if page directory entry is present */
    if (!(page_dir->entries[dir_index] & PAGE_PRESENT)) {
        return NULL;
    }
    
    /* Get page table */
    page_table_t* table = (page_table_t*)PAGE_ENTRY_ADDR(page_dir->entries[dir_index]);
    uint32_t table_index = PAGE_TABLE_INDEX(virt_addr);
    
    return &table->entries[table_index];
}

/* Check if a page is accessible from user mode */
bool vmm_is_user_accessible(page_directory_t* page_dir, uint32_t virt_addr) {
    page_table_entry_t* pte = vmm_get_page_entry(page_dir, virt_addr);
    
    if (!pte || !(*pte & PAGE_PRESENT)) {
        return false;
    }
    
    /* Check both directory and table entry for USER flag */
    uint32_t dir_index = PAGE_DIR_INDEX(virt_addr);
    if (!(page_dir->entries[dir_index] & PAGE_USER)) {
        return false;
    }
    
    return (*pte & PAGE_USER) != 0;
}

/* Validate user pointer for kernel access */
bool validate_user_pointer(const void *ptr, size_t size, int access) {
    if (!ptr) {
        return false;
    }
        
    if (!is_user_range((void*)ptr, size)) {
        return false;
    }
    
    /* Get current process page directory */
    process_t* current = get_current_process();
    if (!current || !current->page_directory) {
        return false;
    }
    
    /* Check each page in the range */
    uint32_t addr = (uint32_t)ptr;
    uint32_t end_addr = addr + size;
    
    for (uint32_t page = addr & PAGE_ALIGN_DOWN(addr); page < end_addr; page += PAGE_SIZE) {
        page_table_entry_t *pte = vmm_get_page_entry(current->page_directory, page);
        
        if (!pte || !(*pte & PAGE_PRESENT) || !(*pte & PAGE_USER)) {
            return false;
        }
            
        if ((access & 0x2) && !(*pte & PAGE_WRITABLE)) {  /* Write access requested */
            return false;
        }
    }
    
    return true;
}

/* Copy data from user space to kernel space safely */
int copy_from_user(void *kernel_dest, const void *user_src, size_t n) {
    /* Validate user pointer */
    if (!validate_user_pointer(user_src, n, 0x1)) {  /* Read access */
        return -1;
    }
    
    /* Perform the copy with page fault protection */
    /* For now, use simple memcpy - in production, this should have exception handling */
    memcpy(kernel_dest, user_src, n);
    
    return 0;
}

/* Copy data from kernel space to user space safely */
int copy_to_user(void *user_dest, const void *kernel_src, size_t n) {
    /* Validate user pointer */
    if (!validate_user_pointer(user_dest, n, 0x2)) {  /* Write access */
        return -1;
    }
    
    /* Ensure destination pages are writable */
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    /* Perform the copy */
    memcpy(user_dest, kernel_src, n);
    
    return 0;
}

/* Safely copy string from user space */
int strncpy_from_user(char *dest, const char *src, size_t n) {
    size_t i;
    
    if (!is_user_address((void*)src)) {
        return -1;
    }
    
    for (i = 0; i < n; i++) {
        if (!is_user_address((void*)(src + i))) {
            return -1;
        }
            
        /* Validate each byte as we copy */
        if (!validate_user_pointer(src + i, 1, 0x1)) {
            return -1;
        }
        
        dest[i] = src[i];
        if (src[i] == '\0') {
            return i;
        }
    }
    
    dest[n-1] = '\0';
    return n-1;
}

/* Enhanced page fault handler with security checks */
void vmm_page_fault_handler_secure(uint32_t error_code, uint32_t fault_addr) {
    /* Decode error code */
    fault_error_t error = {
        .present = (error_code & PF_PRESENT) != 0,
        .write = (error_code & PF_WRITE) != 0,
        .user_mode = (error_code & PF_USER) != 0,
        .reserved_write = (error_code & PF_RESERVED) != 0,
        .instruction_fetch = (error_code & PF_INST_FETCH) != 0
    };
    
    /* Get current process */
    process_t* current = get_current_process();
    
    /* Security check */
    security_violation_t violation = check_memory_access(fault_addr, error, !error.user_mode);
    
    if (violation != SECURITY_OK) {
        handle_security_violation(fault_addr, violation);
        return;
    }
    
    /* Handle legitimate page faults here */
    /* For now, just report and halt */
    terminal_writestring("\nPage Fault (Secure Handler)!\n");
    terminal_writestring("Fault address: 0x");
    terminal_write_hex(fault_addr);
    terminal_writestring("\nError code: 0x");
    terminal_write_hex(error_code);
    terminal_writestring("\n");
    
    if (!error.present) {
        terminal_writestring("Page not present\n");
    }
    if (error.write) {
        terminal_writestring("Write access\n");
    }
    if (error.user_mode) {
        terminal_writestring("User mode access\n");
    }
    
    /* If from user mode, terminate process */
    if (error.user_mode && current) {
        terminal_writestring("Terminating process due to page fault\n");
        process_terminate(current, 11);  /* SIGSEGV */
        return;
    }
    
    /* Kernel fault - panic */
    terminal_writestring("Kernel page fault - system halted\n");
    while (1) {
        __asm__ __volatile__ ("hlt");
    }
}

/* Check memory access for security violations */
security_violation_t check_memory_access(uint32_t addr, fault_error_t error, bool kernel_mode) {
    (void)kernel_mode; /* Suppress unused parameter warning - will be used for SMAP */
    
    /* Check 1: User mode access to kernel space */
    if (error.user_mode && addr < KERNEL_SPACE_END) {
        terminal_writestring("Security: User access to kernel address 0x");
        terminal_write_hex(addr);
        terminal_writestring("\n");
        return SECURITY_VIOLATION_KERNEL_ACCESS;
    }
    
    /* Check 2: Kernel should not access user space without validation */
    /* This would be enhanced with SMAP support */
    
    /* Check 3: Stack overflow detection */
    if (error.user_mode) {
        process_t* current = get_current_process();
        if (current && current->user_stack) {
            /* Simple check - fault below stack with some margin */
            /* For now, assume 8MB stack size */
            #define DEFAULT_USER_STACK_SIZE (8 * 1024 * 1024)
            uint32_t stack_bottom = (uint32_t)current->user_stack - DEFAULT_USER_STACK_SIZE;
            if (addr < stack_bottom - PAGE_SIZE) {
                return SECURITY_VIOLATION_STACK_OVERFLOW;
            }
        }
    }
    
    return SECURITY_OK;
}

/* Handle security violation */
void handle_security_violation(uint32_t fault_addr, security_violation_t violation) {
    process_t* current = get_current_process();
    
    switch (violation) {
        case SECURITY_VIOLATION_KERNEL_ACCESS:
            terminal_writestring("SECURITY VIOLATION: User process attempted to access kernel memory\n");
            terminal_writestring("Address: 0x");
            terminal_write_hex(fault_addr);
            terminal_writestring("\n");
            break;
            
        case SECURITY_VIOLATION_STACK_OVERFLOW:
            terminal_writestring("SECURITY VIOLATION: Stack overflow detected\n");
            break;
            
        default:
            terminal_writestring("SECURITY VIOLATION: Unknown violation type\n");
            break;
    }
    
    /* Terminate the offending process */
    if (current) {
        terminal_writestring("Terminating process ");
        terminal_write_hex(current->pid);
        terminal_writestring(" due to security violation\n");
        process_terminate(current, 9);  /* SIGKILL */
    } else {
        /* No current process - kernel security issue */
        terminal_writestring("KERNEL SECURITY VIOLATION - SYSTEM HALTED\n");
        while (1) {
            __asm__ __volatile__ ("hlt");
        }
    }
}

/* Update page directory entry to ensure proper permissions */
void vmm_update_page_dir_entry_secure(page_directory_t* page_dir, uint32_t index, uint32_t phys_addr, uint32_t flags) {
    /* Ensure kernel page tables are not user accessible */
    if (index < 768 && (flags & PAGE_USER)) {  /* Kernel space is 0-3GB */
        terminal_writestring("VMM: Rejecting USER flag on kernel page table\n");
        flags &= ~PAGE_USER;
    }
    
    page_dir->entries[index] = phys_addr | (flags & 0xFFF);
}

/* Create user-mode compatible page directory */
page_directory_t* vmm_create_user_page_directory(void) {
    page_directory_t* page_dir = vmm_create_page_directory();
    if (!page_dir) {
        return NULL;
    }
    
    /* Ensure kernel page tables are marked as supervisor only */
    for (int i = 0; i < 768; i++) {  /* First 3GB */
        if (page_dir->entries[i] & PAGE_PRESENT) {
            page_dir->entries[i] &= ~PAGE_USER;  /* Clear user bit */
        }
    }
    
    return page_dir;
}