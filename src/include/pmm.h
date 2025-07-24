/*
 * Physical Memory Manager Header
 * 
 * This file defines the interface for the Physical Memory Manager (PMM).
 * The PMM manages physical memory pages using a bitmap allocator.
 */

#ifndef _PMM_H
#define _PMM_H

#include <stdint.h>
#include <stddef.h>
#include "multiboot.h"

/* Page size constant - standard 4KB pages */
#define PAGE_SIZE 4096
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* Convert between addresses and page frame numbers */
#define PAGE_FRAME_NUMBER(addr) ((addr) / PAGE_SIZE)
#define PAGE_FRAME_ADDRESS(frame) ((frame) * PAGE_SIZE)

/* Memory statistics structure */
struct pmm_stats {
    uint32_t total_memory;      /* Total physical memory in bytes */
    uint32_t total_pages;       /* Total number of pages */
    uint32_t used_pages;        /* Number of used pages */
    uint32_t free_pages;        /* Number of free pages */
    uint32_t reserved_pages;    /* Number of reserved pages */
};

/* Initialize the physical memory manager */
void pmm_init(struct multiboot_info *mboot_info);

/* Allocate a single page of physical memory */
uint32_t pmm_alloc_page(void);

/* Allocate multiple contiguous pages */
uint32_t pmm_alloc_pages(size_t count);

/* Free a single page of physical memory */
void pmm_free_page(uint32_t addr);

/* Free multiple contiguous pages */
void pmm_free_pages(uint32_t addr, size_t count);

/* Mark a page as used/allocated */
void pmm_set_page(uint32_t addr);

/* Mark a page as free/available */
void pmm_clear_page(uint32_t addr);

/* Test if a page is allocated */
int pmm_test_page(uint32_t addr);

/* Get the first free page */
uint32_t pmm_first_free(void);

/* Get the first free contiguous block of pages */
uint32_t pmm_first_free_pages(size_t count);

/* Mark a memory region as used */
void pmm_init_region(uint32_t addr, size_t size);

/* Mark a memory region as free */
void pmm_deinit_region(uint32_t addr, size_t size);

/* Get memory statistics */
struct pmm_stats pmm_get_stats(void);

/* Debug: print memory map */
void pmm_print_memory_map(void);

#endif /* _PMM_H */