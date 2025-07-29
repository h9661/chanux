/*
 * ChanUX Kernel Entry Point
 * This is where the kernel begins execution after the bootloader
 */

#include <stdint.h>
#include <stddef.h>
#include "arch/x86/multiboot.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/heap.h"
#include "kernel/pic.h"
#include "kernel/timer.h"
#include "kernel/keyboard.h"
#include "kernel/scheduler.h"

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
    
    /* Initialize Scheduler
     * The scheduler manages process creation, switching, and termination
     */
    scheduler_init();
    
    /* Create test processes */
    terminal_writestring("\nCreating test processes...\n");
    
    /* Test process functions */
    void test_process1(void);
    void test_process2(void);
    void test_process3(void);
    
    /* Create test processes */
    process_create("test1", test_process1);
    process_create("test2", test_process2);
    process_create("test3", test_process3);
    
    /* Kernel initialization complete */
    terminal_writestring("\nWelcome to ChanUX with Process Scheduler!\n");
    terminal_writestring("Scheduler is running with round-robin algorithm.\n\n");
    
    /* Enable interrupts for scheduler */
    __asm__ __volatile__ ("sti");
    
    /* Main kernel loop - scheduler will handle process switching
     * The idle process will run this loop when no other processes are ready
     */
    while (1) {
        __asm__ __volatile__ ("hlt");
    }
}

/* Test Process 1 - Prints messages periodically */
void test_process1(void) {
    terminal_writestring("\n[Process 1] Started!\n");
    
    for (int i = 0; i < 10; i++) {
        terminal_writestring("[Process 1] Working... iteration ");
        terminal_write_dec(i);
        terminal_writestring("\n");
        
        /* Busy wait to simulate work */
        for (volatile int j = 0; j < 10000000; j++);
    }
    
    terminal_writestring("[Process 1] Finished!\n");
    process_exit(0);
}

/* Test Process 2 - Counts and yields */
void test_process2(void) {
    terminal_writestring("\n[Process 2] Started!\n");
    
    for (int i = 0; i < 8; i++) {
        terminal_writestring("[Process 2] Count: ");
        terminal_write_dec(i);
        terminal_writestring(" - yielding CPU\n");
        
        process_yield();
        
        /* Some work after yield */
        for (volatile int j = 0; j < 5000000; j++);
    }
    
    terminal_writestring("[Process 2] Finished!\n");
    process_exit(0);
}

/* Test Process 3 - Uses sleep */
void test_process3(void) {
    terminal_writestring("\n[Process 3] Started!\n");
    
    for (int i = 0; i < 5; i++) {
        terminal_writestring("[Process 3] Sleeping for 100ms... ");
        terminal_write_dec(i);
        terminal_writestring("\n");
        
        timer_sleep(100);
        
        terminal_writestring("[Process 3] Woke up!\n");
    }
    
    terminal_writestring("[Process 3] Finished!\n");
    process_exit(0);
}