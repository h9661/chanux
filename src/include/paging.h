/*
 * x86 Paging Structures and Constants
 * 
 * This file defines the structures and constants for x86 32-bit paging.
 * The x86 uses a two-level page table structure with page directories
 * and page tables to translate virtual addresses to physical addresses.
 */

#ifndef _PAGING_H
#define _PAGING_H

#include <stdint.h>
#include <stddef.h>

/* Page size and alignment macros */
#define PAGE_SIZE 4096
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* Number of entries in page directory and page table */
#define PAGE_DIR_ENTRIES 1024
#define PAGE_TABLE_ENTRIES 1024

/* Virtual address breakdown for 32-bit x86 paging
 * 31-22: Page Directory Index (10 bits)
 * 21-12: Page Table Index (10 bits)
 * 11-0:  Page Offset (12 bits)
 */
#define PAGE_DIR_INDEX(addr) (((addr) >> 22) & 0x3FF)
#define PAGE_TABLE_INDEX(addr) (((addr) >> 12) & 0x3FF)
#define PAGE_OFFSET(addr) ((addr) & 0xFFF)

/* Page entry flags */
#define PAGE_PRESENT    0x001  /* Page is present in memory */
#define PAGE_WRITABLE   0x002  /* Page is writable */
#define PAGE_USER       0x004  /* Page is accessible from user mode */
#define PAGE_WRITE_THROUGH 0x008  /* Write-through caching */
#define PAGE_CACHE_DISABLE 0x010  /* Disable caching */
#define PAGE_ACCESSED   0x020  /* Page has been accessed */
#define PAGE_DIRTY      0x040  /* Page has been written to */
#define PAGE_SIZE_4MB   0x080  /* 4MB page (for page directory entries) */
#define PAGE_GLOBAL     0x100  /* Global page (not flushed on CR3 reload) */

/* Helper macros for page entries */
#define PAGE_ENTRY_ADDR(entry) ((entry) & ~0xFFF)
#define PAGE_ENTRY_FLAGS(entry) ((entry) & 0xFFF)

/* Page directory entry structure
 * Note: We use uint32_t instead of a bitfield for simplicity and performance
 */
typedef uint32_t page_dir_entry_t;

/* Page table entry structure */
typedef uint32_t page_table_entry_t;

/* Page directory structure (array of 1024 entries) */
typedef struct page_directory {
    page_dir_entry_t entries[PAGE_DIR_ENTRIES];
} page_directory_t;

/* Page table structure (array of 1024 entries) */
typedef struct page_table {
    page_table_entry_t entries[PAGE_TABLE_ENTRIES];
} page_table_t;

/* Virtual address structure for easy manipulation */
typedef struct virtual_address {
    uint32_t page_offset : 12;   /* Bits 0-11 */
    uint32_t page_table : 10;    /* Bits 12-21 */
    uint32_t page_dir : 10;      /* Bits 22-31 */
} __attribute__((packed)) virtual_address_t;

/* CR0 register flags for paging */
#define CR0_PAGING_ENABLE  0x80000000  /* Enable paging */
#define CR0_WRITE_PROTECT  0x00010000  /* Write protect (for COW) */

/* CR3 register macros */
#define CR3_PAGE_DIR_ADDR(cr3) ((cr3) & ~0xFFF)
#define CR3_FLAGS(cr3) ((cr3) & 0xFFF)

/* Page fault error codes */
#define PF_PRESENT     0x1   /* Page fault caused by page-present */
#define PF_WRITE       0x2   /* Page fault caused by write access */
#define PF_USER        0x4   /* Page fault from user mode */
#define PF_RESERVED    0x8   /* Page fault caused by reserved bit */
#define PF_INST_FETCH  0x10  /* Page fault during instruction fetch */

#endif /* _PAGING_H */