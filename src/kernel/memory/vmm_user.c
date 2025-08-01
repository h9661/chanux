/*
 * VMM User Mode Extensions
 * 
 * This file provides extensions to the VMM for proper user mode support,
 * including page table creation with correct permissions.
 */

#include <stdint.h>
#include <stddef.h>
#include "kernel/vmm.h"
#include "kernel/pmm.h"
#include "kernel/memory_protection.h"
#include "lib/string.h"

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_write_hex(uint32_t value);

/*
 * Get or create a page table with proper permissions for user pages
 */
page_table_t* get_page_table_secure(page_directory_t* page_dir, uint32_t virt_addr, int create, uint32_t flags) {
    uint32_t dir_index = PAGE_DIR_INDEX(virt_addr);
    
    /* Check if page table exists */
    if (!(page_dir->entries[dir_index] & PAGE_PRESENT)) {
        if (!create) {
            return NULL;
        }
        
        /* Allocate a new page table */
        uint32_t phys_addr = pmm_alloc_page();
        if (phys_addr == 0) {
            terminal_writestring("VMM: Failed to allocate page table\n");
            return NULL;
        }
        
        /* Map the page table temporarily to clear it */
        page_table_t* table = (page_table_t*)phys_addr;
        memset(table, 0, sizeof(page_table_t));
        
        /* Set the page directory entry with appropriate flags */
        uint32_t dir_flags = PAGE_PRESENT | PAGE_WRITABLE;
        
        /* If this is for a user page, add USER flag to directory entry */
        if (flags & PAGE_USER) {
            dir_flags |= PAGE_USER;
        }
        
        page_dir->entries[dir_index] = phys_addr | dir_flags;
    }
    
    /* Return the page table address */
    return (page_table_t*)PAGE_ENTRY_ADDR(page_dir->entries[dir_index]);
}

/*
 * Enhanced map page that uses secure page table creation
 */
void vmm_map_page_with_secure_table(page_directory_t* page_dir, uint32_t virt_addr, 
                                   uint32_t phys_addr, uint32_t flags) {
    /* Align addresses to page boundaries */
    virt_addr = PAGE_ALIGN_DOWN(virt_addr);
    phys_addr = PAGE_ALIGN_DOWN(phys_addr);
    
    /* Get or create page table with proper flags */
    page_table_t* table = get_page_table_secure(page_dir, virt_addr, 1, flags);
    if (!table) {
        return;
    }
    
    /* Set the page table entry */
    uint32_t table_index = PAGE_TABLE_INDEX(virt_addr);
    table->entries[table_index] = phys_addr | flags;
    
    /* Flush TLB for this address if we're modifying current directory */
    if (page_dir == vmm_get_current_directory()) {
        vmm_flush_tlb_entry(virt_addr);
    }
}