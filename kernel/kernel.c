/**
 * =============================================================================
 * Chanux OS - Main Kernel
 * =============================================================================
 * The main kernel entry point and initialization.
 *
 * This is where we set up all the kernel subsystems and start the operating
 * system. The bootloader has already:
 *   - Switched to 64-bit long mode
 *   - Set up initial page tables
 *   - Loaded us into memory
 *
 * Our job now is to:
 *   1. Initialize VGA for output
 *   2. Parse boot information (memory map)
 *   3. Set up our own GDT
 *   4. Set up IDT (interrupts)
 *   5. Initialize memory management
 *   6. Initialize drivers
 *   7. Start the scheduler
 * =============================================================================
 */

#include "include/kernel.h"
#include "drivers/vga/vga.h"

/* =============================================================================
 * Kernel Version Banner
 * =============================================================================
 */

static void print_banner(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("\n");
    kprintf("  ______   __                                       \n");
    kprintf(" /      \\ |  \\                                      \n");
    kprintf("|  $$$$$$\\| $$____    ______   _______   __    __  __    __ \n");
    kprintf("| $$   \\$$| $$    \\  |      \\ |       \\ |  \\  |  \\|  \\  /  \\\n");
    kprintf("| $$      | $$$$$$$\\  \\$$$$$$\\| $$$$$$$\\| $$  | $$ \\$$\\/  $$\n");
    kprintf("| $$   __ | $$  | $$ /      $$| $$  | $$| $$  | $$  >$$  $$ \n");
    kprintf("| $$__/  \\| $$  | $$|  $$$$$$$| $$  | $$| $$__/ $$ /  $$$$\\ \n");
    kprintf(" \\$$    $$| $$  | $$ \\$$    $$| $$  | $$ \\$$    $$|  $$ \\$$\\\n");
    kprintf("  \\$$$$$$  \\$$   \\$$  \\$$$$$$$ \\$$   \\$$  \\$$$$$$  \\$$   \\$$\n");
    kprintf("\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("  Chanux Operating System v%s\n", CHANUX_VERSION_STRING);
    kprintf("  Educational x86_64 OS - Built from Scratch\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("\n");
}

/* =============================================================================
 * Print Memory Map
 * =============================================================================
 */

static void print_memory_map(boot_info_t* boot_info) {
    kprintf("[MEMORY] Memory Map (%d entries):\n", (int)boot_info->memory_map_entries);

    uint64_t total_usable = 0;

    for (uint32_t i = 0; i < boot_info->memory_map_entries; i++) {
        memory_map_entry_t* entry = &boot_info->memory_map[i];

        const char* type_str;
        switch (entry->type) {
            case MEMORY_TYPE_USABLE:
                type_str = "Usable";
                total_usable += entry->length;
                break;
            case MEMORY_TYPE_RESERVED:
                type_str = "Reserved";
                break;
            case MEMORY_TYPE_ACPI_RECLAIMABLE:
                type_str = "ACPI Reclaimable";
                break;
            case MEMORY_TYPE_ACPI_NVS:
                type_str = "ACPI NVS";
                break;
            case MEMORY_TYPE_BAD:
                type_str = "Bad Memory";
                break;
            default:
                type_str = "Unknown";
                break;
        }

        kprintf("  [%d] %p - %p (%s)\n",
            (int)i,
            (void*)entry->base,
            (void*)(entry->base + entry->length - 1),
            type_str);
    }

    kprintf("[MEMORY] Total usable: %d MB\n", (int)(total_usable / (1024 * 1024)));
}

/* =============================================================================
 * Kernel Panic
 * =============================================================================
 */

NORETURN void kernel_panic(const char* file, int line, const char* msg) {
    cli();  /* Disable interrupts */

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n\n");
    kprintf("  *** KERNEL PANIC ***\n");
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    kprintf("  Message: %s\n", msg);
    kprintf("  File:    %s\n", file);
    kprintf("  Line:    %d\n", line);
    kprintf("\n");
    kprintf("  System halted. Please reboot.\n");

    /* Halt forever */
    for (;;) {
        halt();
    }
}

/* =============================================================================
 * Kernel Main Entry Point
 * =============================================================================
 */

void kernel_main(void* boot_info_ptr) {
    /* Cast boot info pointer */
    boot_info_t* boot_info = (boot_info_t*)boot_info_ptr;

    /* ==========================================================================
     * Step 1: Initialize VGA Driver
     * ==========================================================================
     */
    vga_init();
    vga_clear();

    /* Print welcome banner */
    print_banner();

    /* ==========================================================================
     * Step 2: Print Boot Information
     * ==========================================================================
     */
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[OK] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("VGA driver initialized\n");

    /* Print memory map if available */
    if (boot_info && boot_info->memory_map_entries > 0) {
        print_memory_map(boot_info);
    }

    /* ==========================================================================
     * Step 3: Display System Status
     * ==========================================================================
     */
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[OK] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Kernel loaded at physical address 0x100000\n");

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[OK] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Running in 64-bit Long Mode\n");

    /* ==========================================================================
     * Phase 1 Complete - Basic Boot
     * ==========================================================================
     */
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("=================================================\n");
    kprintf("  Phase 1 Complete: Basic Boot Successful!\n");
    kprintf("=================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("\n");
    kprintf("Next steps (Phase 2):\n");
    kprintf("  - Physical Memory Manager (PMM)\n");
    kprintf("  - Virtual Memory Manager (VMM)\n");
    kprintf("  - Kernel Heap (kmalloc/kfree)\n");
    kprintf("\n");
    kprintf("Next steps (Phase 3):\n");
    kprintf("  - Interrupt Descriptor Table (IDT)\n");
    kprintf("  - Exception Handlers\n");
    kprintf("  - Timer (PIT) and Keyboard Drivers\n");
    kprintf("\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("System halted. Chanux is ready for development!\n");
    kprintf("\n");

    /* ==========================================================================
     * Kernel Main Loop
     * ==========================================================================
     * For now, just halt. In later phases, this will be replaced by the
     * scheduler loop.
     */
    for (;;) {
        halt();
    }
}
