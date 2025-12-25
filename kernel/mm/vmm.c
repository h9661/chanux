/**
 * =============================================================================
 * Chanux OS - Virtual Memory Manager Implementation
 * =============================================================================
 * x86-64 4-level paging management.
 * =============================================================================
 */

#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/mm/mm.h"
#include "../include/string.h"
#include "../drivers/vga/vga.h"

/* =============================================================================
 * VMM Internal State
 * =============================================================================
 */

/* Current PML4 table (virtual address) */
static pte_t* vmm_pml4 = NULL;

/* Physical address of current PML4 */
static phys_addr_t vmm_pml4_phys = 0;

/* Statistics */
static uint64_t vmm_pages_mapped = 0;

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/* Read CR3 (page table base) */
static inline phys_addr_t read_cr3_addr(void) {
    return read_cr3() & PTE_ADDR_MASK;
}

/* Allocate and zero a page table */
static pte_t* vmm_alloc_table(void) {
    phys_addr_t phys = pmm_alloc_page();
    if (phys == 0) {
        kprintf("[VMM] ERROR: Cannot allocate page table!\n");
        return NULL;
    }

    pte_t* table = (pte_t*)PHYS_TO_VIRT(phys);
    memset(table, 0, PAGE_SIZE);
    return table;
}

/* Get physical address of a page table */
static inline phys_addr_t vmm_table_phys(pte_t* table) {
    return VIRT_TO_PHYS((virt_addr_t)table);
}

/* =============================================================================
 * VMM Initialization
 * =============================================================================
 */

void vmm_init(void) {
    kprintf("[VMM] Initializing Virtual Memory Manager...\n");

    /* Get bootloader's PML4 */
    phys_addr_t boot_pml4_phys = read_cr3_addr();
    pte_t* boot_pml4 = (pte_t*)PHYS_TO_VIRT(boot_pml4_phys);

    kprintf("[VMM] Bootloader PML4 at physical 0x%x\n", (uint32_t)boot_pml4_phys);

    /* Allocate new PML4 */
    vmm_pml4 = vmm_alloc_table();
    if (!vmm_pml4) {
        PANIC("Cannot allocate PML4");
    }
    vmm_pml4_phys = vmm_table_phys(vmm_pml4);

    kprintf("[VMM] New PML4 at physical 0x%x (virtual 0x%p)\n",
            (uint32_t)vmm_pml4_phys, vmm_pml4);

    /* Copy kernel-space entries (256-511) from bootloader PML4 */
    /* This preserves the higher-half kernel mapping */
    for (int i = 256; i < 512; i++) {
        vmm_pml4[i] = boot_pml4[i];
    }

    /* Keep identity mapping (entry 0) for now */
    /* This is needed until we fully transition to higher-half */
    vmm_pml4[0] = boot_pml4[0];

    /* Set up recursive mapping at PML4[510] */
    /* This allows us to access page tables by dereferencing virtual addresses */
    vmm_pml4[VMM_RECURSIVE_INDEX] = vmm_pml4_phys | PTE_KERNEL_RW;

    kprintf("[VMM] Recursive mapping set at PML4[%d]\n", VMM_RECURSIVE_INDEX);

    /* Switch to new page tables */
    write_cr3(vmm_pml4_phys);

    kprintf("[VMM] Switched to new page tables!\n");
    kprintf("[VMM] Initialization complete.\n");
}

/* =============================================================================
 * Page Mapping
 * =============================================================================
 */

bool vmm_map_page(virt_addr_t virt, phys_addr_t phys, uint64_t flags) {
    /* Validate alignment */
    if (!IS_ALIGNED(virt, PAGE_SIZE) || !IS_ALIGNED(phys, PAGE_SIZE)) {
        kprintf("[VMM] ERROR: Unaligned addresses (virt=0x%p, phys=0x%x)\n",
                (void*)virt, (uint32_t)phys);
        return false;
    }

    /* Extract indices */
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx = PD_INDEX(virt);
    uint64_t pt_idx = PT_INDEX(virt);

    /* Get or create PDPT */
    pte_t* pdpt;
    if (!(vmm_pml4[pml4_idx] & PTE_PRESENT)) {
        pdpt = vmm_alloc_table();
        if (!pdpt) return false;
        vmm_pml4[pml4_idx] = vmm_table_phys(pdpt) | PTE_KERNEL_RW;
    } else {
        pdpt = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(vmm_pml4[pml4_idx]));
    }

    /* Get or create PD */
    pte_t* pd;
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        pd = vmm_alloc_table();
        if (!pd) return false;
        pdpt[pdpt_idx] = vmm_table_phys(pd) | PTE_KERNEL_RW;
    } else {
        /* Check for 1GB huge page */
        if (pdpt[pdpt_idx] & PTE_HUGE) {
            kprintf("[VMM] ERROR: Cannot map 4KB page in 1GB huge page region\n");
            return false;
        }
        pd = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(pdpt[pdpt_idx]));
    }

    /* Get or create PT */
    pte_t* pt;
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        pt = vmm_alloc_table();
        if (!pt) return false;
        pd[pd_idx] = vmm_table_phys(pt) | PTE_KERNEL_RW;
    } else {
        /* Check for 2MB huge page */
        if (pd[pd_idx] & PTE_HUGE) {
            kprintf("[VMM] ERROR: Cannot map 4KB page in 2MB huge page region\n");
            return false;
        }
        pt = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(pd[pd_idx]));
    }

    /* Map the page */
    if (pt[pt_idx] & PTE_PRESENT) {
        /* Page already mapped - update entry */
        pt[pt_idx] = phys | flags;
    } else {
        /* New mapping */
        pt[pt_idx] = phys | flags;
        vmm_pages_mapped++;
    }

    /* Flush TLB for this address */
    vmm_flush_tlb(virt);

    return true;
}

bool vmm_unmap_page(virt_addr_t virt) {
    if (!IS_ALIGNED(virt, PAGE_SIZE)) {
        virt = ALIGN_DOWN(virt, PAGE_SIZE);
    }

    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx = PD_INDEX(virt);
    uint64_t pt_idx = PT_INDEX(virt);

    /* Walk page tables */
    if (!(vmm_pml4[pml4_idx] & PTE_PRESENT)) {
        return false;
    }

    pte_t* pdpt = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(vmm_pml4[pml4_idx]));
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        return false;
    }
    if (pdpt[pdpt_idx] & PTE_HUGE) {
        /* Cannot unmap part of 1GB huge page */
        return false;
    }

    pte_t* pd = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(pdpt[pdpt_idx]));
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        return false;
    }
    if (pd[pd_idx] & PTE_HUGE) {
        /* Cannot unmap part of 2MB huge page */
        return false;
    }

    pte_t* pt = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(pd[pd_idx]));
    if (!(pt[pt_idx] & PTE_PRESENT)) {
        return false;
    }

    /* Clear the entry */
    pt[pt_idx] = 0;
    vmm_pages_mapped--;

    /* Flush TLB */
    vmm_flush_tlb(virt);

    return true;
}

/* =============================================================================
 * Address Translation
 * =============================================================================
 */

phys_addr_t vmm_get_physical(virt_addr_t virt) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx = PD_INDEX(virt);
    uint64_t pt_idx = PT_INDEX(virt);
    uint64_t offset = PAGE_OFFSET(virt);

    /* Walk page tables */
    if (!(vmm_pml4[pml4_idx] & PTE_PRESENT)) {
        return 0;
    }

    pte_t* pdpt = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(vmm_pml4[pml4_idx]));
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        return 0;
    }

    /* Check for 1GB huge page */
    if (pdpt[pdpt_idx] & PTE_HUGE) {
        phys_addr_t base = PTE_GET_ADDR(pdpt[pdpt_idx]);
        uint64_t huge_offset = virt & 0x3FFFFFFF;  /* 30-bit offset for 1GB */
        return base + huge_offset;
    }

    pte_t* pd = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(pdpt[pdpt_idx]));
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        return 0;
    }

    /* Check for 2MB huge page */
    if (pd[pd_idx] & PTE_HUGE) {
        phys_addr_t base = PTE_GET_ADDR(pd[pd_idx]);
        uint64_t huge_offset = virt & 0x1FFFFF;  /* 21-bit offset for 2MB */
        return base + huge_offset;
    }

    pte_t* pt = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(pd[pd_idx]));
    if (!(pt[pt_idx] & PTE_PRESENT)) {
        return 0;
    }

    return PTE_GET_ADDR(pt[pt_idx]) + offset;
}

bool vmm_is_mapped(virt_addr_t virt) {
    return vmm_get_physical(virt) != 0;
}

/* =============================================================================
 * Range Operations
 * =============================================================================
 */

bool vmm_map_range(virt_addr_t virt, phys_addr_t phys, size_t size, uint64_t flags) {
    virt = ALIGN_DOWN(virt, PAGE_SIZE);
    phys = ALIGN_DOWN(phys, PAGE_SIZE);
    size = ALIGN_UP(size, PAGE_SIZE);

    size_t pages = size / PAGE_SIZE;
    for (size_t i = 0; i < pages; i++) {
        if (!vmm_map_page(virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, flags)) {
            /* Rollback on failure */
            for (size_t j = 0; j < i; j++) {
                vmm_unmap_page(virt + j * PAGE_SIZE);
            }
            return false;
        }
    }

    return true;
}

void vmm_unmap_range(virt_addr_t virt, size_t size) {
    virt = ALIGN_DOWN(virt, PAGE_SIZE);
    size = ALIGN_UP(size, PAGE_SIZE);

    size_t pages = size / PAGE_SIZE;
    for (size_t i = 0; i < pages; i++) {
        vmm_unmap_page(virt + i * PAGE_SIZE);
    }
}

/* =============================================================================
 * TLB Management
 * =============================================================================
 */

void vmm_flush_tlb(virt_addr_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_flush_tlb_all(void) {
    /* Reload CR3 to flush all TLB entries */
    write_cr3(read_cr3());
}

/* =============================================================================
 * Debug Functions
 * =============================================================================
 */

phys_addr_t vmm_get_pml4(void) {
    return vmm_pml4_phys;
}

void vmm_debug_print(void) {
    kprintf("\n[VMM] Virtual Memory Statistics:\n");
    kprintf("  PML4 physical:  0x%x\n", (uint32_t)vmm_pml4_phys);
    kprintf("  Pages mapped:   %d\n", (uint32_t)vmm_pages_mapped);
}
