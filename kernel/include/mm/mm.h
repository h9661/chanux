/**
 * =============================================================================
 * Chanux OS - Memory Management Common Header
 * =============================================================================
 * Common definitions and configuration for memory management subsystem.
 * =============================================================================
 */

#ifndef CHANUX_MM_H
#define CHANUX_MM_H

#include "../kernel.h"
#include "../types.h"

/* =============================================================================
 * Memory Layout Configuration
 * =============================================================================
 * Physical Memory Layout (after Phase 2):
 *
 * 0x00000000 - 0x000FFFFF : Reserved (BIOS, VGA, bootloader)
 * 0x00100000 - 0x001FFFFF : Kernel code & data (1MB)
 * 0x00200000 - 0x002FFFFF : PMM Bitmap (1MB, tracks 32GB)
 * 0x00300000 - 0x003FFFFF : Kernel Page Tables (1MB)
 * 0x00400000 - 0x00FFFFFF : Kernel Heap backing (12MB initial)
 * 0x01000000+             : Free physical memory
 */

/* Physical memory regions */
#define MM_RESERVED_END         0x100000        /* First 1MB reserved */
#define MM_KERNEL_START         0x100000        /* Kernel starts at 1MB */
#define MM_KERNEL_SIZE          0x100000        /* Reserve 1MB for kernel */
#define MM_PMM_BITMAP_START     0x200000        /* PMM bitmap at 2MB */
#define MM_PMM_BITMAP_SIZE      0x100000        /* 1MB for bitmap */
#define MM_PAGE_TABLES_START    0x300000        /* Page tables at 3MB */
#define MM_PAGE_TABLES_SIZE     0x100000        /* 1MB for page tables */
#define MM_HEAP_PHYS_START      0x400000        /* Heap backing at 4MB */
#define MM_HEAP_INITIAL_SIZE    (4 * 1024 * 1024)  /* 4MB initial heap */

/* Virtual memory layout */
#define MM_HEAP_VIRT_START      0xFFFFFFFF81000000ULL  /* Heap virtual address */
#define MM_HEAP_VIRT_MAX        0xFFFFFFFF90000000ULL  /* Max heap end */

/* Maximum supported physical memory (32GB) */
#define MM_MAX_PHYS_MEMORY      (32ULL * 1024 * 1024 * 1024)
#define MM_MAX_PAGES            (MM_MAX_PHYS_MEMORY / PAGE_SIZE)

/* =============================================================================
 * Memory Management Statistics
 * =============================================================================
 */

typedef struct {
    /* Physical memory */
    uint64_t total_physical;        /* Total physical memory */
    uint64_t free_physical;         /* Free physical memory */
    uint64_t reserved_physical;     /* Reserved physical memory */

    /* Virtual memory */
    uint64_t kernel_pages_mapped;   /* Kernel pages currently mapped */

    /* Heap */
    uint64_t heap_size;             /* Current heap size */
    uint64_t heap_used;             /* Heap memory in use */
    uint64_t heap_free;             /* Free heap memory */

    /* Allocation counts */
    uint64_t page_allocs;           /* Total page allocations */
    uint64_t page_frees;            /* Total page frees */
    uint64_t kmalloc_calls;         /* kmalloc call count */
    uint64_t kfree_calls;           /* kfree call count */
} mm_stats_t;

/* =============================================================================
 * Memory Management API
 * =============================================================================
 */

/**
 * Initialize the memory management subsystem
 * Must be called early in kernel initialization after VGA is ready
 * @param boot_info Boot information structure from bootloader
 */
void mm_init(boot_info_t* boot_info);

/**
 * Get memory management statistics
 * @param stats Pointer to stats structure to fill
 */
void mm_get_stats(mm_stats_t* stats);

/**
 * Print memory management debug information
 */
void mm_debug_print(void);

#endif /* CHANUX_MM_H */
