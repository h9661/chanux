/**
 * =============================================================================
 * Chanux OS - User Program Linker Script
 * =============================================================================
 * Links user programs to run at 0x400000 (USER_CODE_BASE).
 * =============================================================================
 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

/* User code loads at 4MB */
USER_CODE_BASE = 0x0000000000400000;

SECTIONS
{
    /* Start at user code base address */
    . = USER_CODE_BASE;

    /* Text section (code) */
    .text : ALIGN(4096)
    {
        *(.text.entry)      /* Entry point first */
        *(.text .text.*)    /* All other code */
    }

    /* Read-only data */
    .rodata : ALIGN(4096)
    {
        *(.rodata .rodata.*)
    }

    /* Initialized data */
    .data : ALIGN(4096)
    {
        *(.data .data.*)
    }

    /* Uninitialized data (BSS) */
    .bss : ALIGN(4096)
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
