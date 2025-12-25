/**
 * =============================================================================
 * Chanux OS - Kernel Linker Script
 * =============================================================================
 * Defines the memory layout for the kernel binary.
 *
 * The kernel is loaded at physical address 0x100000 (1MB).
 * For Phase 1, we use identity mapping only (virtual = physical).
 *
 * In later phases, we can switch to higher-half kernel at
 * virtual address 0xFFFFFFFF80000000.
 * =============================================================================
 */

/* Entry point - the _start symbol from boot.asm */
ENTRY(_start)

/* Physical load address (1MB) */
KERNEL_LOAD_ADDR = 0x100000;

SECTIONS
{
    /* Start at physical load address */
    . = KERNEL_LOAD_ADDR;

    /* ==========================================================================
     * Text Section (Code)
     * ==========================================================================
     */
    .text ALIGN(4K) :
    {
        __text_start = .;
        *(.text.boot)
        *(.text)
        *(.text.*)
        __text_end = .;
    }

    /* ==========================================================================
     * Read-Only Data Section
     * ==========================================================================
     */
    .rodata ALIGN(4K) :
    {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        __rodata_end = .;
    }

    /* ==========================================================================
     * Data Section (Initialized data)
     * ==========================================================================
     */
    .data ALIGN(4K) :
    {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    }

    /* ==========================================================================
     * BSS Section (Uninitialized data)
     * ==========================================================================
     * This section is zeroed by the kernel entry code.
     */
    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* End of kernel */
    __kernel_end = .;
    __kernel_size = __kernel_end - KERNEL_LOAD_ADDR;

    /* ==========================================================================
     * Discarded Sections
     * ==========================================================================
     */
    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
