/*
 * Physical Memory Manager Implementation
 * 
 * This file implements a bitmap-based physical memory manager that tracks
 * the allocation status of physical memory pages. Each bit in the bitmap
 * represents one 4KB page of physical memory.
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/pmm.h"
#include "../include/string.h"

/* Bitmap for tracking page allocation status
 * Each bit represents one page: 0 = free, 1 = used
 */
static uint32_t *pmm_bitmap = NULL;
static uint32_t pmm_bitmap_size = 0;  /* Size in 32-bit integers */
static uint32_t pmm_total_pages = 0;
static uint32_t pmm_used_pages = 0;

/* Terminal output functions (defined in terminal.c) */
extern void terminal_writestring(const char* data);
extern void terminal_write_hex(uint32_t value);
extern void terminal_write_dec(uint32_t value);

/* Helper function to convert integer to string */
static void itoa(uint32_t value, char* str, int base) {
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    uint32_t tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    } while (value);

    *ptr-- = '\0';
    
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

/* Print a decimal number */
void terminal_write_dec(uint32_t value) {
    char buffer[12];
    itoa(value, buffer, 10);
    terminal_writestring(buffer);
}

/* Print a hexadecimal number */
void terminal_write_hex(uint32_t value) {
    terminal_writestring("0x");
    char buffer[9];
    itoa(value, buffer, 16);
    terminal_writestring(buffer);
}

/* Set a bit in the bitmap (mark page as used) */
static inline void pmm_bitmap_set(uint32_t bit) {
    pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

/* Clear a bit in the bitmap (mark page as free) */
static inline void pmm_bitmap_clear(uint32_t bit) {
    pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

/* Test if a bit is set in the bitmap */
static inline int pmm_bitmap_test(uint32_t bit) {
    return pmm_bitmap[bit / 32] & (1 << (bit % 32));
}

/* Find the first free bit in the bitmap */
static int pmm_bitmap_first_free(void) {
    for (uint32_t i = 0; i < pmm_bitmap_size; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {  /* If not all bits are set */
            /* Find the first free bit in this 32-bit integer */
            for (int j = 0; j < 32; j++) {
                int bit = i * 32 + j;
                if (bit >= pmm_total_pages) return -1;
                if (!pmm_bitmap_test(bit)) return bit;
            }
        }
    }
    return -1;  /* No free pages */
}

/* Find the first free contiguous block of pages */
static int pmm_bitmap_first_free_pages(size_t count) {
    if (count == 0) return -1;
    if (count == 1) return pmm_bitmap_first_free();
    
    size_t found = 0;
    int start_bit = -1;
    
    for (uint32_t i = 0; i < pmm_total_pages; i++) {
        if (!pmm_bitmap_test(i)) {
            if (found == 0) start_bit = i;
            found++;
            if (found == count) return start_bit;
        } else {
            found = 0;
            start_bit = -1;
        }
    }
    
    return -1;  /* No contiguous block found */
}

/* Initialize the physical memory manager */
void pmm_init(struct multiboot_info *mboot_info) {
    terminal_writestring("Initializing Physical Memory Manager...\n");
    
    /* Check if memory map is available */
    if (!(mboot_info->flags & MULTIBOOT_INFO_MEM_MAP)) {
        terminal_writestring("ERROR: No memory map provided by bootloader!\n");
        return;
    }
    
    /* Calculate total memory from multiboot info
     * mem_upper is memory above 1MB in KB
     */
    uint32_t mem_size = (mboot_info->mem_upper + 1024) * 1024;  /* Convert to bytes */
    pmm_total_pages = mem_size / PAGE_SIZE;
    
    terminal_writestring("Total memory: ");
    terminal_write_dec(mem_size / 1024 / 1024);
    terminal_writestring(" MB (");
    terminal_write_dec(pmm_total_pages);
    terminal_writestring(" pages)\n");
    
    /* Calculate bitmap size and location
     * Place bitmap after the kernel (assume kernel ends at 2MB for now)
     */
    pmm_bitmap_size = (pmm_total_pages + 31) / 32;  /* Round up */
    pmm_bitmap = (uint32_t*)0x200000;  /* 2MB mark */
    
    /* Initialize bitmap - mark all pages as used initially */
    memset(pmm_bitmap, 0xFF, pmm_bitmap_size * sizeof(uint32_t));
    pmm_used_pages = pmm_total_pages;
    
    /* Parse memory map from multiboot info */
    struct multiboot_mmap_entry *mmap = (struct multiboot_mmap_entry *)mboot_info->mmap_addr;
    struct multiboot_mmap_entry *mmap_end = (struct multiboot_mmap_entry *)
        (mboot_info->mmap_addr + mboot_info->mmap_length);
    
    terminal_writestring("Memory map:\n");
    
    while (mmap < mmap_end) {
        /* Print memory region info */
        terminal_writestring("  Region: ");
        terminal_write_hex((uint32_t)mmap->addr);
        terminal_writestring(" - ");
        terminal_write_hex((uint32_t)(mmap->addr + mmap->len));
        terminal_writestring(" (");
        terminal_write_dec((uint32_t)(mmap->len / 1024 / 1024));
        terminal_writestring(" MB) - ");
        
        switch (mmap->type) {
            case MULTIBOOT_MEMORY_AVAILABLE:
                terminal_writestring("Available\n");
                /* Mark available memory as free */
                pmm_deinit_region((uint32_t)mmap->addr, (size_t)mmap->len);
                break;
            case MULTIBOOT_MEMORY_RESERVED:
                terminal_writestring("Reserved\n");
                break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
                terminal_writestring("ACPI Reclaimable\n");
                break;
            case MULTIBOOT_MEMORY_NVS:
                terminal_writestring("ACPI NVS\n");
                break;
            case MULTIBOOT_MEMORY_BADRAM:
                terminal_writestring("Bad RAM\n");
                break;
            default:
                terminal_writestring("Unknown\n");
                break;
        }
        
        /* Move to next entry */
        mmap = (struct multiboot_mmap_entry *)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }
    
    /* Reserve memory used by kernel (0 - 2MB) */
    pmm_init_region(0, 0x200000);
    
    /* Reserve memory used by bitmap */
    uint32_t bitmap_size_bytes = pmm_bitmap_size * sizeof(uint32_t);
    pmm_init_region((uint32_t)pmm_bitmap, bitmap_size_bytes);
    
    /* Print statistics */
    struct pmm_stats stats = pmm_get_stats();
    terminal_writestring("PMM initialized: ");
    terminal_write_dec(stats.free_pages);
    terminal_writestring(" free pages (");
    terminal_write_dec(stats.free_pages * PAGE_SIZE / 1024 / 1024);
    terminal_writestring(" MB)\n");
}

/* Mark a page as used */
void pmm_set_page(uint32_t addr) {
    uint32_t page = PAGE_FRAME_NUMBER(addr);
    if (page >= pmm_total_pages) return;
    
    if (!pmm_bitmap_test(page)) {
        pmm_bitmap_set(page);
        pmm_used_pages++;
    }
}

/* Mark a page as free */
void pmm_clear_page(uint32_t addr) {
    uint32_t page = PAGE_FRAME_NUMBER(addr);
    if (page >= pmm_total_pages) return;
    
    if (pmm_bitmap_test(page)) {
        pmm_bitmap_clear(page);
        pmm_used_pages--;
    }
}

/* Test if a page is allocated */
int pmm_test_page(uint32_t addr) {
    uint32_t page = PAGE_FRAME_NUMBER(addr);
    if (page >= pmm_total_pages) return 1;  /* Out of bounds = used */
    return pmm_bitmap_test(page);
}

/* Allocate a single page */
uint32_t pmm_alloc_page(void) {
    int page = pmm_bitmap_first_free();
    if (page == -1) return 0;  /* No memory available */
    
    pmm_bitmap_set(page);
    pmm_used_pages++;
    
    return PAGE_FRAME_ADDRESS(page);
}

/* Allocate multiple contiguous pages */
uint32_t pmm_alloc_pages(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();
    
    int start_page = pmm_bitmap_first_free_pages(count);
    if (start_page == -1) return 0;  /* No contiguous block available */
    
    /* Mark all pages as used */
    for (size_t i = 0; i < count; i++) {
        pmm_bitmap_set(start_page + i);
    }
    pmm_used_pages += count;
    
    return PAGE_FRAME_ADDRESS(start_page);
}

/* Free a single page */
void pmm_free_page(uint32_t addr) {
    pmm_clear_page(addr);
}

/* Free multiple contiguous pages */
void pmm_free_pages(uint32_t addr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        pmm_clear_page(addr + i * PAGE_SIZE);
    }
}

/* Get the first free page */
uint32_t pmm_first_free(void) {
    int page = pmm_bitmap_first_free();
    if (page == -1) return 0;
    return PAGE_FRAME_ADDRESS(page);
}

/* Get the first free contiguous block of pages */
uint32_t pmm_first_free_pages(size_t count) {
    int start_page = pmm_bitmap_first_free_pages(count);
    if (start_page == -1) return 0;
    return PAGE_FRAME_ADDRESS(start_page);
}

/* Mark a memory region as used */
void pmm_init_region(uint32_t addr, size_t size) {
    uint32_t start_page = PAGE_FRAME_NUMBER(addr);
    uint32_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;  /* Round up */
    
    for (uint32_t i = 0; i < num_pages; i++) {
        if (start_page + i < pmm_total_pages) {
            if (!pmm_bitmap_test(start_page + i)) {
                pmm_bitmap_set(start_page + i);
                pmm_used_pages++;
            }
        }
    }
}

/* Mark a memory region as free */
void pmm_deinit_region(uint32_t addr, size_t size) {
    uint32_t start_page = PAGE_FRAME_NUMBER(addr);
    uint32_t num_pages = size / PAGE_SIZE;  /* Don't round up for freeing */
    
    for (uint32_t i = 0; i < num_pages; i++) {
        if (start_page + i < pmm_total_pages) {
            if (pmm_bitmap_test(start_page + i)) {
                pmm_bitmap_clear(start_page + i);
                pmm_used_pages--;
            }
        }
    }
}

/* Get memory statistics */
struct pmm_stats pmm_get_stats(void) {
    struct pmm_stats stats;
    stats.total_memory = pmm_total_pages * PAGE_SIZE;
    stats.total_pages = pmm_total_pages;
    stats.used_pages = pmm_used_pages;
    stats.free_pages = pmm_total_pages - pmm_used_pages;
    stats.reserved_pages = 0;  /* TODO: Track reserved pages separately */
    return stats;
}

/* Debug: print memory map */
void pmm_print_memory_map(void) {
    terminal_writestring("Physical Memory Map:\n");
    terminal_writestring("Total pages: ");
    terminal_write_dec(pmm_total_pages);
    terminal_writestring("\nUsed pages: ");
    terminal_write_dec(pmm_used_pages);
    terminal_writestring("\nFree pages: ");
    terminal_write_dec(pmm_total_pages - pmm_used_pages);
    terminal_writestring("\n");
    
    /* Print first few pages status */
    terminal_writestring("First 10 pages: ");
    for (int i = 0; i < 10 && i < pmm_total_pages; i++) {
        terminal_writestring(pmm_bitmap_test(i) ? "U" : "F");
    }
    terminal_writestring("\n");
}