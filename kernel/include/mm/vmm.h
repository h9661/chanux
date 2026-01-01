/**
 * =============================================================================
 * Chanux OS - Virtual Memory Manager Header
 * =============================================================================
 * x86-64 4-level paging management.
 *
 * Virtual Address Structure (48-bit canonical):
 *   Bits 47-39: PML4 index (9 bits)
 *   Bits 38-30: PDPT index (9 bits)
 *   Bits 29-21: PD index (9 bits)
 *   Bits 20-12: PT index (9 bits)
 *   Bits 11-0:  Page offset (12 bits)
 * =============================================================================
 */

#ifndef CHANUX_VMM_H
#define CHANUX_VMM_H

#include "../kernel.h"
#include "../types.h"

/* =============================================================================
 * Page Table Entry Flags
 * =============================================================================
 */

#define PTE_PRESENT         (1ULL << 0)     /* Page is present in memory */
#define PTE_WRITABLE        (1ULL << 1)     /* Page is writable */
#define PTE_USER            (1ULL << 2)     /* User-mode accessible */
#define PTE_WRITETHROUGH    (1ULL << 3)     /* Write-through caching */
#define PTE_NOCACHE         (1ULL << 4)     /* Disable caching */
#define PTE_ACCESSED        (1ULL << 5)     /* Page has been accessed */
#define PTE_DIRTY           (1ULL << 6)     /* Page has been written to */
#define PTE_HUGE            (1ULL << 7)     /* 2MB/1GB huge page */
#define PTE_GLOBAL          (1ULL << 8)     /* Global page (survives TLB flush) */
#define PTE_NX              (1ULL << 63)    /* No execute (requires NX support) */

/* Common flag combinations */
#define PTE_KERNEL_RW       (PTE_PRESENT | PTE_WRITABLE)
#define PTE_KERNEL_RO       (PTE_PRESENT)
#define PTE_KERNEL_RWX      (PTE_PRESENT | PTE_WRITABLE)  /* No NX */
#define PTE_USER_RW         (PTE_PRESENT | PTE_WRITABLE | PTE_USER)
#define PTE_USER_RO         (PTE_PRESENT | PTE_USER)

/* Physical address mask (bits 12-51) */
#define PTE_ADDR_MASK       0x000FFFFFFFFFF000ULL

/* Extract physical address from page table entry */
#define PTE_GET_ADDR(pte)   ((pte) & PTE_ADDR_MASK)

/* =============================================================================
 * Virtual Address Index Extraction
 * =============================================================================
 */

#define PML4_INDEX(addr)    (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr)    (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)      (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)      (((addr) >> 12) & 0x1FF)
#define PAGE_OFFSET(addr)   ((addr) & 0xFFF)

/* =============================================================================
 * Page Table Types
 * =============================================================================
 */

typedef uint64_t pte_t;             /* Page table entry */
typedef pte_t page_table_t[512];    /* 512 entries per level */

/* =============================================================================
 * VMM Configuration
 * =============================================================================
 */

/* Recursive mapping index (PML4[510] points to itself) */
#define VMM_RECURSIVE_INDEX     510

/* Recursive mapping addresses for accessing page tables */
#define VMM_RECURSIVE_BASE      0xFFFFFF0000000000ULL

/* =============================================================================
 * VMM API
 * =============================================================================
 */

/**
 * Initialize the Virtual Memory Manager
 * Creates new page tables and switches from bootloader tables.
 */
void vmm_init(void);

/**
 * Map a virtual address to a physical address
 *
 * @param virt  Virtual address to map (must be page-aligned)
 * @param phys  Physical address to map to (must be page-aligned)
 * @param flags Page table entry flags (PTE_*)
 * @return true on success, false on failure (out of memory)
 */
bool vmm_map_page(virt_addr_t virt, phys_addr_t phys, uint64_t flags);

/**
 * Unmap a virtual address
 *
 * @param virt Virtual address to unmap (must be page-aligned)
 * @return true if page was mapped and is now unmapped
 */
bool vmm_unmap_page(virt_addr_t virt);

/**
 * Get the physical address for a virtual address
 *
 * @param virt Virtual address to translate
 * @return Physical address, or 0 if not mapped
 */
phys_addr_t vmm_get_physical(virt_addr_t virt);

/**
 * Check if a virtual address is mapped
 *
 * @param virt Virtual address to check
 * @return true if mapped, false otherwise
 */
bool vmm_is_mapped(virt_addr_t virt);

/**
 * Map a range of virtual addresses to physical addresses
 *
 * @param virt  Starting virtual address (must be page-aligned)
 * @param phys  Starting physical address (must be page-aligned)
 * @param size  Number of bytes to map (will be rounded up to page boundary)
 * @param flags Page table entry flags
 * @return true on success, false on failure
 */
bool vmm_map_range(virt_addr_t virt, phys_addr_t phys, size_t size, uint64_t flags);

/**
 * Unmap a range of virtual addresses
 *
 * @param virt Starting virtual address
 * @param size Number of bytes to unmap
 */
void vmm_unmap_range(virt_addr_t virt, size_t size);

/**
 * Flush TLB entry for a specific virtual address
 *
 * @param virt Virtual address to invalidate
 */
void vmm_flush_tlb(virt_addr_t virt);

/**
 * Flush entire TLB (reload CR3)
 */
void vmm_flush_tlb_all(void);

/**
 * Get the current PML4 physical address
 *
 * @return Physical address of current PML4
 */
phys_addr_t vmm_get_pml4(void);

/**
 * Print VMM debug information
 */
void vmm_debug_print(void);

/* =============================================================================
 * User Mode Address Space Management (Phase 5)
 * =============================================================================
 */

/* User space address limits */
#define USER_SPACE_START    0x0000000000400000ULL   /* 4MB - avoid null pages */
#define USER_SPACE_END      0x0000800000000000ULL   /* End of lower canonical half */
#define USER_STACK_TOP      0x00007FFFFFFFE000ULL   /* Just below end of user space */
#define USER_STACK_SIZE     (16 * PAGE_SIZE)        /* 64KB user stack */

/**
 * Create a new address space (PML4) for a user process.
 * The new address space has kernel mappings copied (higher half)
 * but no user-space mappings.
 *
 * @return Physical address of new PML4, or 0 on failure
 */
phys_addr_t vmm_create_address_space(void);

/**
 * Destroy an address space and free all associated page tables.
 * Does NOT free the physical pages that were mapped - caller must do that.
 *
 * @param pml4_phys Physical address of PML4 to destroy
 */
void vmm_destroy_address_space(phys_addr_t pml4_phys);

/**
 * Switch to a different address space.
 *
 * @param pml4_phys Physical address of PML4 to switch to
 */
void vmm_switch_address_space(phys_addr_t pml4_phys);

/**
 * Map a page in a specific address space with user permissions.
 *
 * @param pml4_phys Physical address of target PML4
 * @param virt      Virtual address to map
 * @param phys      Physical address to map to
 * @param flags     Page table entry flags (should include PTE_USER)
 * @return true on success, false on failure
 */
bool vmm_map_user_page(phys_addr_t pml4_phys, virt_addr_t virt,
                       phys_addr_t phys, uint64_t flags);

/**
 * Clone kernel mappings from one address space to another.
 * Only copies the higher-half (kernel) PML4 entries.
 *
 * @param dst_pml4_phys Destination PML4 physical address
 * @param src_pml4_phys Source PML4 physical address
 */
void vmm_clone_kernel_mappings(phys_addr_t dst_pml4_phys, phys_addr_t src_pml4_phys);

#endif /* CHANUX_VMM_H */
