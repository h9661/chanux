/*
 * ChanUX Kernel Entry Point
 * This is where the kernel begins execution after the bootloader
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/multiboot.h"
#include "../include/pmm.h"

/* External function declarations for system initialization */
void gdt_install(void);        /* Set up Global Descriptor Table */
void idt_install(void);        /* Set up Interrupt Descriptor Table */
void terminal_initialize(void); /* Initialize VGA text mode terminal */
void terminal_writestring(const char* data); /* Write string to terminal */

/*
 * kernel_main - Main entry point for the kernel
 * @magic: Multiboot magic number (should be 0x2BADB002)
 * @addr: Physical address of Multiboot information structure
 *
 * This function is called by the bootloader after setting up the environment.
 * It initializes core system components and enters an idle loop.
 */
void kernel_main(uint32_t magic, uint32_t addr) {
    /* Initialize the terminal for output before anything else */
    terminal_initialize();
    terminal_writestring("ChanUX kernel booting...\n");
    
    /* Verify multiboot magic number */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        terminal_writestring("ERROR: Invalid multiboot magic number!\n");
        return;
    }
    
    /* Cast address to multiboot info structure */
    struct multiboot_info *mboot_info = (struct multiboot_info *)addr;
    
    /* Install Global Descriptor Table
     * The GDT defines memory segments for code and data in protected mode
     * This is essential for proper memory protection and privilege levels
     */
    gdt_install();
    terminal_writestring("GDT installed\n");
    
    /* Install Interrupt Descriptor Table
     * The IDT defines handlers for CPU interrupts and exceptions
     * This is needed for handling hardware interrupts and system calls
     */
    idt_install();
    terminal_writestring("IDT installed\n");
    
    /* Initialize Physical Memory Manager
     * The PMM manages allocation of physical memory pages using a bitmap
     * It needs the multiboot info to detect available memory regions
     */
    pmm_init(mboot_info);
    
    /* Run PMM unit tests */
    void pmm_run_tests(void);
    pmm_run_tests();
    
    /* Print memory statistics */
    terminal_writestring("\nFinal memory state:\n");
    pmm_print_memory_map();
    
    /* Kernel initialization complete */
    terminal_writestring("\nWelcome to ChanUX!\n");
    
    /* Main kernel loop - halt CPU when idle to save power
     * The HLT instruction stops the CPU until an interrupt occurs
     * This is more efficient than a busy-wait loop
     */
    while (1) {
        __asm__ __volatile__ ("hlt");
    }
}