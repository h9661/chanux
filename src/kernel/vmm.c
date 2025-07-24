/*
 * Virtual Memory Manager Implementation
 * 
 * This file implements virtual memory management using x86 paging.
 * It handles page directory/table creation, virtual-to-physical mapping,
 * and memory allocation with proper page table management.
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/string.h"

/* Current page directory */
static page_directory_t* current_directory = NULL;

/* Kernel page directory (created at init) */
static page_directory_t* kernel_directory = NULL;

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_write_hex(uint32_t value);
extern void terminal_write_dec(uint32_t value);

/* Assembly functions for enabling paging */
extern void enable_paging(uint32_t page_dir_phys);
extern uint32_t read_cr2(void);
extern uint32_t read_cr3(void);
extern void write_cr3(uint32_t value);
extern void invlpg(uint32_t addr);

/*
 * Helper function to get or create a page table for a virtual address
 */
static page_table_t* get_page_table(page_directory_t* page_dir, uint32_t virt_addr, int create) {
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
        
        /* Set the page directory entry */
        page_dir->entries[dir_index] = phys_addr | PAGE_PRESENT | PAGE_WRITABLE;
    }
    
    /* Return the page table address */
    return (page_table_t*)PAGE_ENTRY_ADDR(page_dir->entries[dir_index]);
}

/*
 * Initialize the virtual memory manager
 */
void vmm_init(void) {
    terminal_writestring("Initializing Virtual Memory Manager...\n");
    
    /* Allocate kernel page directory */
    uint32_t dir_phys = pmm_alloc_page();
    if (dir_phys == 0) {
        terminal_writestring("VMM: Failed to allocate kernel page directory!\n");
        return;
    }
    
    kernel_directory = (page_directory_t*)dir_phys;
    memset(kernel_directory, 0, sizeof(page_directory_t));
    
    /* Identity map the first 4MB of physical memory (kernel space)
     * This ensures the kernel continues to work after paging is enabled
     */
    terminal_writestring("Identity mapping kernel memory (0-4MB)...\n");
    for (uint32_t i = 0; i < 0x400000; i += PAGE_SIZE) {
        vmm_map_page(kernel_directory, i, i, PAGE_PRESENT | PAGE_WRITABLE);
    }
    
    /* Identity map the VGA buffer */
    vmm_map_page(kernel_directory, 0xB8000, 0xB8000, PAGE_PRESENT | PAGE_WRITABLE);
    
    /* Set current directory */
    current_directory = kernel_directory;
    
    /* Enable paging */
    terminal_writestring("Enabling paging...\n");
    enable_paging(dir_phys);
    
    terminal_writestring("Virtual Memory Manager initialized\n");
}

/*
 * Create a new page directory
 */
page_directory_t* vmm_create_page_directory(void) {
    uint32_t phys_addr = pmm_alloc_page();
    if (phys_addr == 0) {
        return NULL;
    }
    
    page_directory_t* page_dir = (page_directory_t*)phys_addr;
    memset(page_dir, 0, sizeof(page_directory_t));
    
    /* Copy kernel mappings to new directory */
    if (kernel_directory) {
        /* Copy the first 256 entries (1GB) for kernel space */
        for (int i = 0; i < 256; i++) {
            page_dir->entries[i] = kernel_directory->entries[i];
        }
    }
    
    return page_dir;
}

/*
 * Free a page directory and all its page tables
 */
void vmm_free_page_directory(page_directory_t* page_dir) {
    if (page_dir == kernel_directory) {
        return;  /* Never free the kernel directory */
    }
    
    /* Free all user-space page tables (entries 256-1023) */
    for (int i = 256; i < PAGE_DIR_ENTRIES; i++) {
        if (page_dir->entries[i] & PAGE_PRESENT) {
            uint32_t table_phys = PAGE_ENTRY_ADDR(page_dir->entries[i]);
            pmm_free_page(table_phys);
        }
    }
    
    /* Free the directory itself */
    pmm_free_page((uint32_t)page_dir);
}

/*
 * Switch to a different page directory
 */
void vmm_switch_page_directory(page_directory_t* page_dir) {
    current_directory = page_dir;
    write_cr3((uint32_t)page_dir);
}

/*
 * Map a virtual address to a physical address
 */
void vmm_map_page(page_directory_t* page_dir, uint32_t virt_addr, 
                  uint32_t phys_addr, uint32_t flags) {
    /* Align addresses to page boundaries */
    virt_addr = PAGE_ALIGN_DOWN(virt_addr);
    phys_addr = PAGE_ALIGN_DOWN(phys_addr);
    
    /* Get or create page table */
    page_table_t* table = get_page_table(page_dir, virt_addr, 1);
    if (!table) {
        return;
    }
    
    /* Set the page table entry */
    uint32_t table_index = PAGE_TABLE_INDEX(virt_addr);
    table->entries[table_index] = phys_addr | flags;
    
    /* Flush TLB for this address if we're modifying current directory */
    if (page_dir == current_directory) {
        vmm_flush_tlb_entry(virt_addr);
    }
}

/*
 * Unmap a virtual address
 */
void vmm_unmap_page(page_directory_t* page_dir, uint32_t virt_addr) {
    virt_addr = PAGE_ALIGN_DOWN(virt_addr);
    
    /* Get page table */
    page_table_t* table = get_page_table(page_dir, virt_addr, 0);
    if (!table) {
        return;
    }
    
    /* Clear the page table entry */
    uint32_t table_index = PAGE_TABLE_INDEX(virt_addr);
    table->entries[table_index] = 0;
    
    /* Flush TLB for this address if we're modifying current directory */
    if (page_dir == current_directory) {
        vmm_flush_tlb_entry(virt_addr);
    }
}

/*
 * Check if a virtual address is mapped
 */
int vmm_is_page_mapped(page_directory_t* page_dir, uint32_t virt_addr) {
    page_table_t* table = get_page_table(page_dir, virt_addr, 0);
    if (!table) {
        return 0;
    }
    
    uint32_t table_index = PAGE_TABLE_INDEX(virt_addr);
    return (table->entries[table_index] & PAGE_PRESENT) ? 1 : 0;
}

/*
 * Get the physical address for a virtual address
 */
uint32_t vmm_get_physical_addr(page_directory_t* page_dir, uint32_t virt_addr) {
    page_table_t* table = get_page_table(page_dir, virt_addr, 0);
    if (!table) {
        return 0;
    }
    
    uint32_t table_index = PAGE_TABLE_INDEX(virt_addr);
    if (!(table->entries[table_index] & PAGE_PRESENT)) {
        return 0;
    }
    
    uint32_t phys_page = PAGE_ENTRY_ADDR(table->entries[table_index]);
    uint32_t offset = PAGE_OFFSET(virt_addr);
    return phys_page + offset;
}

/*
 * Map a range of pages
 */
void vmm_map_range(page_directory_t* page_dir, uint32_t virt_start,
                   uint32_t phys_start, uint32_t size, uint32_t flags) {
    uint32_t virt_addr = PAGE_ALIGN_DOWN(virt_start);
    uint32_t phys_addr = PAGE_ALIGN_DOWN(phys_start);
    size = PAGE_ALIGN_UP(size);
    
    for (uint32_t offset = 0; offset < size; offset += PAGE_SIZE) {
        vmm_map_page(page_dir, virt_addr + offset, phys_addr + offset, flags);
    }
}

/*
 * Unmap a range of pages
 */
void vmm_unmap_range(page_directory_t* page_dir, uint32_t virt_start, uint32_t size) {
    uint32_t virt_addr = PAGE_ALIGN_DOWN(virt_start);
    size = PAGE_ALIGN_UP(size);
    
    for (uint32_t offset = 0; offset < size; offset += PAGE_SIZE) {
        vmm_unmap_page(page_dir, virt_addr + offset);
    }
}

/*
 * Identity map a physical memory region
 */
void vmm_identity_map(page_directory_t* page_dir, uint32_t phys_start,
                      uint32_t size, uint32_t flags) {
    vmm_map_range(page_dir, phys_start, phys_start, size, flags);
}

/*
 * Allocate virtual memory pages
 */
uint32_t vmm_alloc_page(page_directory_t* page_dir, uint32_t virt_addr, uint32_t flags) {
    /* Allocate physical page */
    uint32_t phys_addr = pmm_alloc_page();
    if (phys_addr == 0) {
        return 0;
    }
    
    /* Map it */
    vmm_map_page(page_dir, virt_addr, phys_addr, flags);
    return virt_addr;
}

/*
 * Allocate multiple contiguous virtual pages
 */
uint32_t vmm_alloc_pages(page_directory_t* page_dir, uint32_t virt_addr, 
                        size_t count, uint32_t flags) {
    /* Allocate physical pages */
    uint32_t phys_addr = pmm_alloc_pages(count);
    if (phys_addr == 0) {
        return 0;
    }
    
    /* Map them */
    for (size_t i = 0; i < count; i++) {
        vmm_map_page(page_dir, virt_addr + i * PAGE_SIZE, 
                     phys_addr + i * PAGE_SIZE, flags);
    }
    
    return virt_addr;
}

/*
 * Free virtual memory pages
 */
void vmm_free_page(page_directory_t* page_dir, uint32_t virt_addr) {
    /* Get physical address */
    uint32_t phys_addr = vmm_get_physical_addr(page_dir, virt_addr);
    if (phys_addr == 0) {
        return;
    }
    
    /* Unmap and free */
    vmm_unmap_page(page_dir, virt_addr);
    pmm_free_page(phys_addr);
}

/*
 * Free multiple contiguous virtual pages
 */
void vmm_free_pages(page_directory_t* page_dir, uint32_t virt_addr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        vmm_free_page(page_dir, virt_addr + i * PAGE_SIZE);
    }
}

/*
 * Flush TLB for a specific address
 */
void vmm_flush_tlb_entry(uint32_t virt_addr) {
    invlpg(virt_addr);
}

/*
 * Flush entire TLB
 */
void vmm_flush_tlb(void) {
    write_cr3(read_cr3());
}

/*
 * Get current page directory
 */
page_directory_t* vmm_get_current_directory(void) {
    return current_directory;
}

/*
 * Clone a page directory (simplified version - only copies structure)
 */
page_directory_t* vmm_clone_directory(page_directory_t* src) {
    page_directory_t* new_dir = vmm_create_page_directory();
    if (!new_dir) {
        return NULL;
    }
    
    /* Note: This is a simplified implementation that only copies
     * the directory structure, not the actual page contents.
     * A full implementation would need to implement copy-on-write
     * or actually copy all mapped pages.
     */
    
    return new_dir;
}

/*
 * Page fault handler
 */
void vmm_page_fault_handler(uint32_t error_code, uint32_t fault_addr) {
    terminal_writestring("\nPage Fault!\n");
    terminal_writestring("Fault address: ");
    terminal_write_hex(fault_addr);
    terminal_writestring("\nError code: ");
    terminal_write_hex(error_code);
    terminal_writestring("\n");
    
    if (!(error_code & PF_PRESENT)) {
        terminal_writestring("Page not present\n");
    }
    if (error_code & PF_WRITE) {
        terminal_writestring("Write access violation\n");
    }
    if (error_code & PF_USER) {
        terminal_writestring("User mode access\n");
    }
    
    /* For now, just halt on page fault */
    terminal_writestring("System halted\n");
    while (1) {
        __asm__ __volatile__ ("hlt");
    }
}

/*
 * Debug: print page mappings
 */
void vmm_print_mappings(page_directory_t* page_dir, uint32_t virt_start, uint32_t virt_end) {
    terminal_writestring("Virtual memory mappings:\n");
    
    for (uint32_t virt = virt_start; virt < virt_end; virt += PAGE_SIZE) {
        if (vmm_is_page_mapped(page_dir, virt)) {
            uint32_t phys = vmm_get_physical_addr(page_dir, virt);
            terminal_writestring("  ");
            terminal_write_hex(virt);
            terminal_writestring(" -> ");
            terminal_write_hex(phys);
            terminal_writestring("\n");
        }
    }
}