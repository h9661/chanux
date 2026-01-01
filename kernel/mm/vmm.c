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
#include "../include/debug.h"
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
 * Huge Page Splitting
 * =============================================================================
 */

/**
 * Split a 2MB huge page into 512 x 4KB pages.
 * This is needed when the bootloader sets up huge pages but we need
 * fine-grained control for heap or other kernel allocations.
 */
static bool vmm_split_huge_page(pte_t* pd, uint64_t pd_idx) {
    pte_t huge_entry = pd[pd_idx];

    /* Get the physical base address of the 2MB region */
    phys_addr_t huge_base = PTE_GET_ADDR(huge_entry);

    /* Extract flags (excluding address and HUGE bit) */
    uint64_t flags = huge_entry & ~(PTE_ADDR_MASK | PTE_HUGE);

    /* Allocate a new page table */
    pte_t* pt = vmm_alloc_table();
    if (!pt) {
        kprintf("[VMM] ERROR: Cannot allocate PT for huge page split\n");
        return false;
    }

    /* Map all 512 x 4KB pages with the same flags */
    for (int i = 0; i < 512; i++) {
        pt[i] = (huge_base + i * PAGE_SIZE) | flags;
    }

    /* Replace PD entry: point to new PT instead of huge page */
    /* Use kernel RW flags for the PD entry itself */
    pd[pd_idx] = vmm_table_phys(pt) | PTE_KERNEL_RW;

    /* Flush entire TLB since we changed the page structure */
    vmm_flush_tlb_all();

    kprintf("[VMM] Split 2MB huge page at phys 0x%x into 4KB pages\n",
            (uint32_t)huge_base);

    return true;
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
        /* Check for 2MB huge page - split it if needed */
        if (pd[pd_idx] & PTE_HUGE) {
            if (!vmm_split_huge_page(pd, pd_idx)) {
                kprintf("[VMM] ERROR: Cannot split 2MB huge page\n");
                return false;
            }
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

/* =============================================================================
 * User Mode Address Space Management (Phase 5)
 * =============================================================================
 */

phys_addr_t vmm_create_address_space(void) {
    /* Allocate a new PML4 */
    phys_addr_t new_pml4_phys = pmm_alloc_page();
    if (new_pml4_phys == 0) {
        return 0;
    }

    /* Get virtual address to access the new PML4 */
    pte_t* new_pml4 = (pte_t*)PHYS_TO_VIRT(new_pml4_phys);

    /* Clear the entire PML4 */
    for (int i = 0; i < 512; i++) {
        new_pml4[i] = 0;
    }

    /* Copy kernel mappings from current PML4 (entries 256-511 for higher half) */
    pte_t* current_pml4 = (pte_t*)PHYS_TO_VIRT(vmm_pml4_phys);
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = current_pml4[i];
    }

    /*
     * IMPORTANT: Also copy identity mapping (entry 0) for kernel code.
     * The kernel is currently linked at physical 0x100000, not higher-half.
     * Without this, the kernel code becomes unmapped after CR3 switch.
     * TODO: Remove this once kernel is linked at 0xFFFFFFFF80100000.
     */
    new_pml4[0] = current_pml4[0];

    return new_pml4_phys;
}

void vmm_destroy_address_space(phys_addr_t pml4_phys) {
    if (pml4_phys == 0 || pml4_phys == vmm_pml4_phys) {
        /* Don't destroy kernel's address space */
        return;
    }

    pte_t* pml4 = (pte_t*)PHYS_TO_VIRT(pml4_phys);

    /* Only free user-space page tables (entries 0-255) */
    for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        if (!(pml4[pml4_idx] & PTE_PRESENT)) {
            continue;
        }

        phys_addr_t pdpt_phys = PTE_GET_ADDR(pml4[pml4_idx]);
        pte_t* pdpt = (pte_t*)PHYS_TO_VIRT(pdpt_phys);

        for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
            if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
                continue;
            }

            phys_addr_t pd_phys = PTE_GET_ADDR(pdpt[pdpt_idx]);
            pte_t* pd = (pte_t*)PHYS_TO_VIRT(pd_phys);

            for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
                if (!(pd[pd_idx] & PTE_PRESENT)) {
                    continue;
                }

                if (pd[pd_idx] & PTE_HUGE) {
                    /* 2MB huge page - skip, don't free */
                    continue;
                }

                phys_addr_t pt_phys = PTE_GET_ADDR(pd[pd_idx]);
                /* Free the page table */
                pmm_free_page(pt_phys);
            }

            /* Free the page directory */
            pmm_free_page(pd_phys);
        }

        /* Free the PDPT */
        pmm_free_page(pdpt_phys);
    }

    /* Free the PML4 itself */
    pmm_free_page(pml4_phys);
}

void vmm_switch_address_space(phys_addr_t pml4_phys) {
    if (pml4_phys != 0 && pml4_phys != read_cr3()) {
        write_cr3(pml4_phys);
    }
}

bool vmm_map_user_page(phys_addr_t pml4_phys, virt_addr_t virt,
                       phys_addr_t phys, uint64_t flags) {
    /* Ensure address is in user space */
    if (virt >= USER_SPACE_END) {
        return false;
    }

    /* Ensure user flag is set */
    flags |= PTE_USER;

    pte_t* pml4 = (pte_t*)PHYS_TO_VIRT(pml4_phys);

    /* Get indices */
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx = PD_INDEX(virt);
    uint64_t pt_idx = PT_INDEX(virt);

    DBG_VMM("[VMM-U] map 0x%llx: idx=%d/%d/%d/%d pml4[%d]=0x%llx\n",
            (unsigned long long)virt, (int)pml4_idx, (int)pdpt_idx,
            (int)pd_idx, (int)pt_idx, (int)pml4_idx, pml4[pml4_idx]);

    /* Walk/create PDPT - create copy if shared with kernel (no USER flag) */
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        phys_addr_t new_pdpt = pmm_alloc_page();
        if (new_pdpt == 0) {
            return false;
        }
        pte_t* pdpt_ptr = (pte_t*)PHYS_TO_VIRT(new_pdpt);
        for (int i = 0; i < 512; i++) {
            pdpt_ptr[i] = 0;
        }
        pml4[pml4_idx] = new_pdpt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else if (!(pml4[pml4_idx] & PTE_USER)) {
        /* Entry copied from kernel - create a copy with USER flag */
        DBG_VMM("[VMM-U] Copying PDPT (pml4[%d] has no USER)\n", (int)pml4_idx);
        phys_addr_t old_pdpt_phys = PTE_GET_ADDR(pml4[pml4_idx]);
        phys_addr_t new_pdpt = pmm_alloc_page();
        if (new_pdpt == 0) {
            return false;
        }
        pte_t* old_pdpt = (pte_t*)PHYS_TO_VIRT(old_pdpt_phys);
        pte_t* new_pdpt_ptr = (pte_t*)PHYS_TO_VIRT(new_pdpt);
        for (int i = 0; i < 512; i++) {
            new_pdpt_ptr[i] = old_pdpt[i];
        }
        pml4[pml4_idx] = new_pdpt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        DBG_VMM("[VMM-U] pml4[%d] now = 0x%llx\n", (int)pml4_idx, pml4[pml4_idx]);
    }

    pte_t* pdpt = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(pml4[pml4_idx]));

    /* Walk/create PD - create copy if shared with kernel (no USER flag) */
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        phys_addr_t new_pd = pmm_alloc_page();
        if (new_pd == 0) {
            return false;
        }
        pte_t* pd_ptr = (pte_t*)PHYS_TO_VIRT(new_pd);
        for (int i = 0; i < 512; i++) {
            pd_ptr[i] = 0;
        }
        pdpt[pdpt_idx] = new_pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else if (!(pdpt[pdpt_idx] & PTE_USER)) {
        /* Entry copied from kernel - create a copy with USER flag */
        DBG_VMM("[VMM-U] Copying PD (pdpt[%d] has no USER)\n", (int)pdpt_idx);
        phys_addr_t old_pd_phys = PTE_GET_ADDR(pdpt[pdpt_idx]);
        phys_addr_t new_pd = pmm_alloc_page();
        if (new_pd == 0) {
            return false;
        }
        pte_t* old_pd = (pte_t*)PHYS_TO_VIRT(old_pd_phys);
        pte_t* new_pd_ptr = (pte_t*)PHYS_TO_VIRT(new_pd);
        for (int i = 0; i < 512; i++) {
            new_pd_ptr[i] = old_pd[i];
        }
        pdpt[pdpt_idx] = new_pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        DBG_VMM("[VMM-U] pdpt[%d] now = 0x%llx\n", (int)pdpt_idx, pdpt[pdpt_idx]);
    }

    pte_t* pd = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(pdpt[pdpt_idx]));

    DBG_VMM("[VMM-U] pd[%d] = 0x%llx (HUGE=%d)\n",
            (int)pd_idx, pd[pd_idx], (pd[pd_idx] & PTE_HUGE) ? 1 : 0);

    /* Check for 2MB huge page that needs splitting */
    if ((pd[pd_idx] & PTE_PRESENT) && (pd[pd_idx] & PTE_HUGE)) {
        DBG_VMM("[VMM-U] Splitting 2MB huge page at pd[%d]\n", (int)pd_idx);
        pte_t huge_entry = pd[pd_idx];
        phys_addr_t huge_base = PTE_GET_ADDR(huge_entry);

        /* Allocate new PT for split 4KB pages */
        phys_addr_t new_pt = pmm_alloc_page();
        if (new_pt == 0) {
            return false;
        }
        pte_t* pt_ptr = (pte_t*)PHYS_TO_VIRT(new_pt);

        /* Split: map 512 x 4KB pages with USER flag for accessibility */
        uint64_t split_flags = PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        for (int i = 0; i < 512; i++) {
            pt_ptr[i] = (huge_base + i * PAGE_SIZE) | split_flags;
        }

        /* Replace PD entry with pointer to new PT */
        pd[pd_idx] = new_pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        vmm_flush_tlb_all();
    }

    /* Walk/create PT */
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        phys_addr_t new_pt = pmm_alloc_page();
        if (new_pt == 0) {
            return false;
        }
        pte_t* pt_ptr = (pte_t*)PHYS_TO_VIRT(new_pt);
        for (int i = 0; i < 512; i++) {
            pt_ptr[i] = 0;
        }
        pd[pd_idx] = new_pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }

    pte_t* pt = (pte_t*)PHYS_TO_VIRT(PTE_GET_ADDR(pd[pd_idx]));

    /* Map the page */
    pt[pt_idx] = (phys & PTE_ADDR_MASK) | flags;

    return true;
}

void vmm_clone_kernel_mappings(phys_addr_t dst_pml4_phys, phys_addr_t src_pml4_phys) {
    pte_t* dst_pml4 = (pte_t*)PHYS_TO_VIRT(dst_pml4_phys);
    pte_t* src_pml4 = (pte_t*)PHYS_TO_VIRT(src_pml4_phys);

    /* Copy kernel mappings (entries 256-511 for higher half) */
    for (int i = 256; i < 512; i++) {
        dst_pml4[i] = src_pml4[i];
    }
}
