/*
 * Virtual Memory Manager Header
 * 
 * This file defines the interface for the Virtual Memory Manager (VMM).
 * The VMM handles virtual-to-physical address mapping, page allocation,
 * and page table management.
 */

#ifndef _VMM_H
#define _VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/paging.h"

/* Virtual memory regions */
#define KERNEL_VIRT_BASE    0x00000000  /* Kernel starts at 0 (identity mapped) */
#define KERNEL_HEAP_START   0x04000000  /* Kernel heap starts at 64MB */
#define KERNEL_HEAP_SIZE    0x04000000  /* 64MB for kernel heap */
#define USER_SPACE_START    0x08000000  /* User space starts at 128MB */
#define USER_SPACE_END      0x3FFFF000  /* User space ends at ~1GB */

/* Initialize the virtual memory manager */
void vmm_init(void);

/* Allocate a new page directory */
page_directory_t* vmm_create_page_directory(void);

/* Free a page directory and all its page tables */
void vmm_free_page_directory(page_directory_t* page_dir);

/* Switch to a different page directory */
void vmm_switch_page_directory(page_directory_t* page_dir);

/* Map a virtual address to a physical address */
void vmm_map_page(page_directory_t* page_dir, uint32_t virt_addr, 
                  uint32_t phys_addr, uint32_t flags);

/* Unmap a virtual address */
void vmm_unmap_page(page_directory_t* page_dir, uint32_t virt_addr);

/* Check if a virtual address is mapped */
int vmm_is_page_mapped(page_directory_t* page_dir, uint32_t virt_addr);

/* Get the physical address for a virtual address */
uint32_t vmm_get_physical_addr(page_directory_t* page_dir, uint32_t virt_addr);

/* Map a range of pages */
void vmm_map_range(page_directory_t* page_dir, uint32_t virt_start,
                   uint32_t phys_start, uint32_t size, uint32_t flags);

/* Unmap a range of pages */
void vmm_unmap_range(page_directory_t* page_dir, uint32_t virt_start, uint32_t size);

/* Identity map a physical memory region (virt = phys) */
void vmm_identity_map(page_directory_t* page_dir, uint32_t phys_start,
                      uint32_t size, uint32_t flags);

/* Allocate virtual memory pages */
uint32_t vmm_alloc_page(page_directory_t* page_dir, uint32_t virt_addr, uint32_t flags);

/* Allocate multiple contiguous virtual pages */
uint32_t vmm_alloc_pages(page_directory_t* page_dir, uint32_t virt_addr, 
                        size_t count, uint32_t flags);

/* Free virtual memory pages */
void vmm_free_page(page_directory_t* page_dir, uint32_t virt_addr);

/* Free multiple contiguous virtual pages */
void vmm_free_pages(page_directory_t* page_dir, uint32_t virt_addr, size_t count);

/* Flush TLB for a specific address */
void vmm_flush_tlb_entry(uint32_t virt_addr);

/* Flush entire TLB */
void vmm_flush_tlb(void);

/* Get current page directory */
page_directory_t* vmm_get_current_directory(void);

/* Clone a page directory (for process creation) */
page_directory_t* vmm_clone_directory(page_directory_t* src);

/* Page fault handler */
void vmm_page_fault_handler(uint32_t error_code, uint32_t fault_addr);

/* Debug: print page mappings */
void vmm_print_mappings(page_directory_t* page_dir, uint32_t virt_start, uint32_t virt_end);

/* Security-enhanced functions */
void vmm_map_page_secure(page_directory_t* page_dir, uint32_t virt_addr, 
                        uint32_t phys_addr, uint32_t flags);
bool vmm_is_user_accessible(page_directory_t* page_dir, uint32_t virt_addr);
page_table_entry_t* vmm_get_page_entry(page_directory_t* page_dir, uint32_t virt_addr);
page_directory_t* vmm_create_user_page_directory(void);

#endif /* _VMM_H */