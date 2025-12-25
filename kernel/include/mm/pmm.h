/**
 * =============================================================================
 * Chanux OS - Physical Memory Manager Header
 * =============================================================================
 * Bitmap-based physical page frame allocator.
 *
 * The PMM tracks physical memory pages using a bitmap where:
 * - 0 = page is free
 * - 1 = page is used/reserved
 *
 * The bitmap is located at physical address 0x200000 and can track
 * up to 32GB of physical RAM (8 million 4KB pages = 1MB bitmap).
 * =============================================================================
 */

#ifndef CHANUX_PMM_H
#define CHANUX_PMM_H

#include "../kernel.h"
#include "../types.h"
#include "mm.h"

/* =============================================================================
 * PMM Statistics
 * =============================================================================
 */

typedef struct {
    uint64_t total_pages;       /* Total physical pages in system */
    uint64_t free_pages;        /* Currently free pages */
    uint64_t used_pages;        /* Currently allocated pages */
    uint64_t reserved_pages;    /* Reserved pages (kernel, BIOS, etc.) */
    uint64_t total_memory;      /* Total memory in bytes */
    uint64_t free_memory;       /* Free memory in bytes */
} pmm_stats_t;

/* =============================================================================
 * PMM API
 * =============================================================================
 */

/**
 * Initialize the Physical Memory Manager
 * Parses the E820 memory map and sets up the allocation bitmap.
 *
 * @param boot_info Boot information structure containing memory map
 */
void pmm_init(boot_info_t* boot_info);

/**
 * Allocate a single physical page frame
 *
 * @return Physical address of allocated page, or 0 on failure
 */
phys_addr_t pmm_alloc_page(void);

/**
 * Allocate multiple contiguous physical page frames
 *
 * @param count Number of contiguous pages to allocate
 * @return Physical address of first page, or 0 on failure
 */
phys_addr_t pmm_alloc_pages(size_t count);

/**
 * Free a single physical page frame
 *
 * @param addr Physical address of page to free (must be page-aligned)
 */
void pmm_free_page(phys_addr_t addr);

/**
 * Free multiple contiguous physical page frames
 *
 * @param addr Physical address of first page (must be page-aligned)
 * @param count Number of pages to free
 */
void pmm_free_pages(phys_addr_t addr, size_t count);

/**
 * Mark a page as reserved (cannot be allocated)
 * Used for kernel, hardware, and BIOS reserved regions.
 *
 * @param addr Physical address of page to reserve
 */
void pmm_reserve_page(phys_addr_t addr);

/**
 * Mark multiple contiguous pages as reserved
 *
 * @param addr Physical address of first page
 * @param count Number of pages to reserve
 */
void pmm_reserve_pages(phys_addr_t addr, size_t count);

/**
 * Mark a page as free (undo reservation)
 *
 * @param addr Physical address of page to unreserve
 */
void pmm_unreserve_page(phys_addr_t addr);

/**
 * Check if a physical page is free
 *
 * @param addr Physical address to check
 * @return true if page is free, false otherwise
 */
bool pmm_is_page_free(phys_addr_t addr);

/**
 * Get PMM statistics
 *
 * @param stats Pointer to stats structure to fill
 */
void pmm_get_stats(pmm_stats_t* stats);

/**
 * Print PMM debug information
 * Shows memory map, allocation statistics, and bitmap info.
 */
void pmm_debug_print(void);

#endif /* CHANUX_PMM_H */
