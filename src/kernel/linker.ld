/*
 * ChanUX Kernel Linker Script
 * 
 * This linker script defines the memory layout for the ChanUX kernel.
 * It specifies how different sections of the compiled code and data
 * should be arranged in memory when the kernel is loaded.
 */

/* Define the entry point of the kernel - the first instruction to execute */
ENTRY(_start)

/* Define the sections and their memory layout */
SECTIONS
{
    /* Set the starting address to 1MB (0x100000)
     * This is the traditional location for kernel loading in x86 systems
     * The area below 1MB is reserved for BIOS, bootloader, and legacy devices */
    . = 1M;

    /* Code section - contains executable instructions
     * BLOCK(4K): Ensures section starts at 4KB boundary
     * ALIGN(4K): Aligns the section to 4KB for paging compatibility */
    .text BLOCK(4K) : ALIGN(4K)
    {
        /* Multiboot header must come first for GRUB to recognize the kernel */
        *(.multiboot)
        
        /* All code from compiled object files */
        *(.text)
    }

    /* Read-only data section - contains constants and string literals
     * Separated from .text for security (can be marked non-executable) */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Initialized data section - contains global/static variables with initial values
     * This data is stored in the binary and loaded into memory */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* Uninitialized data section - contains global/static variables without initial values
     * This section doesn't take space in the binary file, only in memory
     * The bootloader/kernel is responsible for zeroing this area */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        /* COMMON symbols - uninitialized global variables from C */
        *(COMMON)
        
        /* Explicitly declared .bss section data */
        *(.bss)
    }
    
    /* The linker will automatically update the location counter (.)
     * to point to the end of the kernel after all sections are placed.
     * This can be used to determine kernel size at runtime. */
}