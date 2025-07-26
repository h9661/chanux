/*
 * ChanUX Kernel Entry Point
 * This is where the kernel begins execution after the bootloader
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/multiboot.h"
#include "../include/pmm.h"
#include "../include/vmm.h"
#include "../include/heap.h"
#include "../include/pic.h"
#include "../include/timer.h"
#include "../include/keyboard.h"

/* External function declarations for system initialization */
void gdt_install(void);        /* Set up Global Descriptor Table */
void idt_install(void);        /* Set up Interrupt Descriptor Table */
void terminal_initialize(void); /* Initialize VGA text mode terminal */
void terminal_writestring(const char* data); /* Write string to terminal */
void terminal_write_dec(uint32_t value); /* Write decimal number to terminal */
void terminal_write_hex(uint8_t value); /* Write hexadecimal number to terminal */
uint64_t timer_get_ticks(void); /* Get current timer tick count */
void pic_print_status(void);    /* Print PIC status */

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
    
    /* Initialize Programmable Interrupt Controller
     * The PIC manages hardware interrupts (IRQs) and remaps them
     * to avoid conflicts with CPU exceptions
     */
    pic_init();
    terminal_writestring("PIC initialized\n");
    
    /* Initialize Physical Memory Manager
     * The PMM manages allocation of physical memory pages using a bitmap
     * It needs the multiboot info to detect available memory regions
     */
    pmm_init(mboot_info);
    
    /* Initialize Virtual Memory Manager
     * The VMM provides virtual memory mapping using x86 paging
     * It sets up page tables and enables memory protection
     */
    vmm_init();
    
    
    /* Test paging by accessing mapped memory */
    terminal_writestring("\nTesting virtual memory access...\n");
    
    /* Map a test page */
    uint32_t test_virt = 0x20000000;  /* 512MB */
    uint32_t test_result = vmm_alloc_page(vmm_get_current_directory(), 
                                         test_virt, PAGE_PRESENT | PAGE_WRITABLE);
    
    if (test_result != 0) {
        /* Write to the virtual address */
        uint32_t* test_ptr = (uint32_t*)test_virt;
        *test_ptr = 0xDEADBEEF;
        
        /* Read it back */
        if (*test_ptr == 0xDEADBEEF) {
            terminal_writestring("Virtual memory write/read successful!\n");
        } else {
            terminal_writestring("Virtual memory test failed!\n");
        }
        
        /* Clean up */
        vmm_free_page(vmm_get_current_directory(), test_virt);
    }
    
    /* Initialize Heap Allocator
     * The heap provides dynamic memory allocation (malloc/free)
     * It uses virtual memory pages allocated by the VMM
     */
    heap_init();
    
    /* Initialize Timer Driver
     * The timer provides system timing and delays
     * It must be initialized after PIC for interrupt handling
     */
    timer_init(TIMER_DEFAULT_FREQ);
    
    /* Initialize Keyboard Driver
     * The keyboard driver handles PS/2 keyboard input
     * It must be initialized after PIC for interrupt handling
     */
    keyboard_init();
    
    /* Initialize System Call Interface
     * This sets up interrupt 0x80 for system calls from user space
     */
    void syscall_init(void);
    syscall_init();
    
    /* Kernel initialization complete */
    terminal_writestring("\nWelcome to ChanUX with Virtual Memory, Heap, Interrupts, Timer, Keyboard, and System Calls!\n");
    
    /* Main kernel loop - halt CPU when idle to save power
     * The HLT instruction stops the CPU until an interrupt occurs
     * This is more efficient than a busy-wait loop
     */
    while (1) {
        __asm__ __volatile__ ("hlt");
    }
}