/**
 * =============================================================================
 * Chanux OS - Physical Memory Manager Implementation
 * =============================================================================
 * Bitmap-based physical page frame allocator.
 * =============================================================================
 */

#include "../include/mm/pmm.h"
#include "../include/mm/mm.h"
#include "../include/string.h"
#include "../drivers/vga/vga.h"

/* =============================================================================
 * PMM Internal State
 * =============================================================================
 */

/* Bitmap is stored at physical 0x200000, accessed via higher-half mapping */
static uint8_t* pmm_bitmap = NULL;

/* Statistics */
static uint64_t pmm_total_pages = 0;
static uint64_t pmm_free_count = 0;
static uint64_t pmm_reserved_pages = 0;

/* First free page hint for faster allocation */
static uint64_t pmm_first_free_hint = 0;

/* Total detected memory */
static uint64_t pmm_total_memory = 0;

/* =============================================================================
 * Bitmap Manipulation Macros
 * =============================================================================
 */

#define BITMAP_INDEX(page)      ((page) / 8)
#define BITMAP_OFFSET(page)     ((page) % 8)
#define BITMAP_SET(page)        (pmm_bitmap[BITMAP_INDEX(page)] |= (1 << BITMAP_OFFSET(page)))
#define BITMAP_CLEAR(page)      (pmm_bitmap[BITMAP_INDEX(page)] &= ~(1 << BITMAP_OFFSET(page)))
#define BITMAP_TEST(page)       (pmm_bitmap[BITMAP_INDEX(page)] & (1 << BITMAP_OFFSET(page)))

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/* Convert physical address to page frame number */
static inline uint64_t addr_to_pfn(phys_addr_t addr) {
    return addr / PAGE_SIZE;
}

/* Convert page frame number to physical address */
static inline phys_addr_t pfn_to_addr(uint64_t pfn) {
    return pfn * PAGE_SIZE;
}

/* Memory type to string for debug output */
static const char* memory_type_str(uint32_t type) {
    switch (type) {
        case MEMORY_TYPE_USABLE:            return "Usable";
        case MEMORY_TYPE_RESERVED:          return "Reserved";
        case MEMORY_TYPE_ACPI_RECLAIMABLE:  return "ACPI Reclaimable";
        case MEMORY_TYPE_ACPI_NVS:          return "ACPI NVS";
        case MEMORY_TYPE_BAD:               return "Bad Memory";
        default:                            return "Unknown";
    }
}

/* =============================================================================
 * PMM Initialization
 * =============================================================================
 */

void pmm_init(boot_info_t* boot_info) {
    kprintf("[PMM] Initializing Physical Memory Manager...\n");

    /* Set up bitmap pointer (physical 0x200000 mapped in higher-half) */
    pmm_bitmap = (uint8_t*)PHYS_TO_VIRT(MM_PMM_BITMAP_START);

    /* Initially mark ALL pages as used (safe default) */
    memset(pmm_bitmap, 0xFF, MM_PMM_BITMAP_SIZE);

    kprintf("[PMM] Bitmap at physical 0x%x (virtual 0x%p)\n",
            MM_PMM_BITMAP_START, pmm_bitmap);

    /* Process E820 memory map */
    if (!boot_info || boot_info->memory_map_entries == 0) {
        kprintf("[PMM] ERROR: No memory map available!\n");
        PANIC("No memory map from bootloader");
    }

    kprintf("[PMM] Processing %d E820 memory map entries:\n",
            boot_info->memory_map_entries);

    /* Walk E820 map and mark usable regions as free */
    for (uint32_t i = 0; i < boot_info->memory_map_entries; i++) {
        memory_map_entry_t* entry = &boot_info->memory_map[i];

        kprintf("  [%d] 0x%08x%08x - 0x%08x%08x (%s)\n",
                i,
                (uint32_t)(entry->base >> 32), (uint32_t)entry->base,
                (uint32_t)((entry->base + entry->length) >> 32),
                (uint32_t)(entry->base + entry->length),
                memory_type_str(entry->type));

        if (entry->type == MEMORY_TYPE_USABLE) {
            /* Calculate page-aligned region */
            phys_addr_t start = ALIGN_UP(entry->base, PAGE_SIZE);
            phys_addr_t end = ALIGN_DOWN(entry->base + entry->length, PAGE_SIZE);

            if (end > start) {
                /* Mark pages as free */
                for (phys_addr_t addr = start; addr < end; addr += PAGE_SIZE) {
                    uint64_t pfn = addr_to_pfn(addr);
                    if (pfn < MM_MAX_PAGES) {
                        BITMAP_CLEAR(pfn);
                        pmm_free_count++;
                    }
                }

                pmm_total_memory += (end - start);
            }
        }
    }

    pmm_total_pages = pmm_free_count;

    /* Reserve critical regions */
    kprintf("[PMM] Reserving system regions...\n");

    /* Reserve first 1MB (BIOS, VGA, bootloader, etc.) */
    kprintf("  - First 1MB (BIOS/VGA/bootloader)\n");
    pmm_reserve_pages(0, MM_RESERVED_END / PAGE_SIZE);

    /* Reserve kernel (1MB at 0x100000) */
    kprintf("  - Kernel (1MB at 0x100000)\n");
    pmm_reserve_pages(MM_KERNEL_START, MM_KERNEL_SIZE / PAGE_SIZE);

    /* Reserve PMM bitmap (1MB at 0x200000) */
    kprintf("  - PMM bitmap (1MB at 0x200000)\n");
    pmm_reserve_pages(MM_PMM_BITMAP_START, MM_PMM_BITMAP_SIZE / PAGE_SIZE);

    /* Reserve page tables region (1MB at 0x300000) */
    kprintf("  - Page tables (1MB at 0x300000)\n");
    pmm_reserve_pages(MM_PAGE_TABLES_START, MM_PAGE_TABLES_SIZE / PAGE_SIZE);

    /* Find first free page */
    pmm_first_free_hint = 0;
    for (uint64_t pfn = 0; pfn < MM_MAX_PAGES; pfn++) {
        if (!BITMAP_TEST(pfn)) {
            pmm_first_free_hint = pfn;
            break;
        }
    }

    kprintf("[PMM] Initialization complete!\n");
    kprintf("  Total memory:    %d MB\n", (uint32_t)(pmm_total_memory / (1024 * 1024)));
    kprintf("  Free pages:      %d (%d MB)\n",
            (uint32_t)pmm_free_count,
            (uint32_t)(pmm_free_count * PAGE_SIZE / (1024 * 1024)));
    kprintf("  Reserved pages:  %d (%d MB)\n",
            (uint32_t)pmm_reserved_pages,
            (uint32_t)(pmm_reserved_pages * PAGE_SIZE / (1024 * 1024)));
    kprintf("  First free page: 0x%x\n", (uint32_t)pfn_to_addr(pmm_first_free_hint));
}

/* =============================================================================
 * Page Allocation
 * =============================================================================
 */

phys_addr_t pmm_alloc_page(void) {
    /* Search from hint position */
    for (uint64_t pfn = pmm_first_free_hint; pfn < MM_MAX_PAGES; pfn++) {
        if (!BITMAP_TEST(pfn)) {
            BITMAP_SET(pfn);
            pmm_free_count--;
            pmm_first_free_hint = pfn + 1;
            return pfn_to_addr(pfn);
        }
    }

    /* Wrap around and search from beginning */
    for (uint64_t pfn = 0; pfn < pmm_first_free_hint; pfn++) {
        if (!BITMAP_TEST(pfn)) {
            BITMAP_SET(pfn);
            pmm_free_count--;
            pmm_first_free_hint = pfn + 1;
            return pfn_to_addr(pfn);
        }
    }

    /* Out of memory */
    kprintf("[PMM] ERROR: Out of physical memory!\n");
    return 0;
}

phys_addr_t pmm_alloc_pages(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();

    /* Find contiguous free pages */
    uint64_t start_pfn = 0;
    size_t found = 0;

    for (uint64_t pfn = pmm_first_free_hint; pfn < MM_MAX_PAGES && found < count; pfn++) {
        if (!BITMAP_TEST(pfn)) {
            if (found == 0) {
                start_pfn = pfn;
            }
            found++;
        } else {
            found = 0;
        }
    }

    /* If not found from hint, search from beginning */
    if (found < count) {
        found = 0;
        for (uint64_t pfn = 0; pfn < pmm_first_free_hint && found < count; pfn++) {
            if (!BITMAP_TEST(pfn)) {
                if (found == 0) {
                    start_pfn = pfn;
                }
                found++;
            } else {
                found = 0;
            }
        }
    }

    if (found < count) {
        kprintf("[PMM] ERROR: Cannot allocate %d contiguous pages!\n", (int)count);
        return 0;
    }

    /* Mark pages as used */
    for (size_t i = 0; i < count; i++) {
        BITMAP_SET(start_pfn + i);
    }
    pmm_free_count -= count;

    /* Update hint */
    pmm_first_free_hint = start_pfn + count;

    return pfn_to_addr(start_pfn);
}

/* =============================================================================
 * Page Deallocation
 * =============================================================================
 */

void pmm_free_page(phys_addr_t addr) {
    if (addr == 0) return;

    if (!IS_ALIGNED(addr, PAGE_SIZE)) {
        kprintf("[PMM] WARNING: Freeing unaligned address 0x%x\n", (uint32_t)addr);
        addr = ALIGN_DOWN(addr, PAGE_SIZE);
    }

    uint64_t pfn = addr_to_pfn(addr);
    if (pfn >= MM_MAX_PAGES) {
        kprintf("[PMM] WARNING: Address 0x%x out of range\n", (uint32_t)addr);
        return;
    }

    if (!BITMAP_TEST(pfn)) {
        kprintf("[PMM] WARNING: Double free at 0x%x\n", (uint32_t)addr);
        return;
    }

    BITMAP_CLEAR(pfn);
    pmm_free_count++;

    /* Update hint if this page is before current hint */
    if (pfn < pmm_first_free_hint) {
        pmm_first_free_hint = pfn;
    }
}

void pmm_free_pages(phys_addr_t addr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        pmm_free_page(addr + i * PAGE_SIZE);
    }
}

/* =============================================================================
 * Page Reservation
 * =============================================================================
 */

void pmm_reserve_page(phys_addr_t addr) {
    uint64_t pfn = addr_to_pfn(addr);
    if (pfn >= MM_MAX_PAGES) return;

    if (!BITMAP_TEST(pfn)) {
        BITMAP_SET(pfn);
        pmm_free_count--;
        pmm_reserved_pages++;
    }
}

void pmm_reserve_pages(phys_addr_t addr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        pmm_reserve_page(addr + i * PAGE_SIZE);
    }
}

void pmm_unreserve_page(phys_addr_t addr) {
    uint64_t pfn = addr_to_pfn(addr);
    if (pfn >= MM_MAX_PAGES) return;

    if (BITMAP_TEST(pfn)) {
        BITMAP_CLEAR(pfn);
        pmm_free_count++;
        if (pmm_reserved_pages > 0) {
            pmm_reserved_pages--;
        }
    }
}

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

bool pmm_is_page_free(phys_addr_t addr) {
    uint64_t pfn = addr_to_pfn(addr);
    if (pfn >= MM_MAX_PAGES) return false;
    return !BITMAP_TEST(pfn);
}

void pmm_get_stats(pmm_stats_t* stats) {
    if (!stats) return;

    stats->total_pages = pmm_total_pages;
    stats->free_pages = pmm_free_count;
    stats->used_pages = pmm_total_pages - pmm_free_count;
    stats->reserved_pages = pmm_reserved_pages;
    stats->total_memory = pmm_total_memory;
    stats->free_memory = pmm_free_count * PAGE_SIZE;
}

void pmm_debug_print(void) {
    pmm_stats_t stats;
    pmm_get_stats(&stats);

    kprintf("\n[PMM] Physical Memory Statistics:\n");
    kprintf("  Total pages:     %d\n", (uint32_t)stats.total_pages);
    kprintf("  Free pages:      %d\n", (uint32_t)stats.free_pages);
    kprintf("  Used pages:      %d\n", (uint32_t)stats.used_pages);
    kprintf("  Reserved pages:  %d\n", (uint32_t)stats.reserved_pages);
    kprintf("  Total memory:    %d MB\n", (uint32_t)(stats.total_memory / (1024 * 1024)));
    kprintf("  Free memory:     %d MB\n", (uint32_t)(stats.free_memory / (1024 * 1024)));
}
