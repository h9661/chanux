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
#include "include/mm/mm.h"
#include "include/mm/pmm.h"
#include "include/mm/vmm.h"
#include "include/mm/heap.h"
#include "include/gdt.h"
#include "include/interrupts/idt.h"
#include "include/interrupts/irq.h"
#include "include/drivers/pic.h"
#include "include/drivers/pit.h"
#include "include/drivers/keyboard.h"
#include "include/proc/process.h"
#include "include/proc/sched.h"
#include "include/syscall/syscall.h"
#include "include/user/user.h"
#include "include/fs/vfs.h"
#include "include/fs/ramfs.h"
#include "include/string.h"
#include "drivers/vga/vga.h"

/* =============================================================================
 * Embedded Shell Program
 * =============================================================================
 * These symbols are created by objcopy from the user shell binary.
 */
extern const char _user_shell_start[];
extern const char _user_shell_end[];
extern const char _user_shell_size[];

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
 * Memory Management Initialization
 * =============================================================================
 */

void mm_init(boot_info_t* boot_info) {
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("[MM] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Initializing Memory Management Subsystem...\n");
    kprintf("\n");

    /* Step 1: Physical Memory Manager */
    pmm_init(boot_info);

    /* Step 2: Virtual Memory Manager */
    vmm_init();

    /* Step 3: Kernel Heap */
    heap_init();

    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[MM] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Memory Management initialized successfully!\n");
}

/* =============================================================================
 * Filesystem Initialization (Phase 6)
 * =============================================================================
 */

static void fs_init(void) {
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("[FS] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Initializing filesystem...\n");

    /* Initialize VFS and mount RAMFS as root */
    vfs_init();

    /* Create initial directories */
    vfs_mkdir("/bin");
    vfs_mkdir("/home");
    vfs_mkdir("/tmp");

    /* Create a welcome file */
    vnode_t* file = NULL;
    if (vfs_lookup("/hello.txt", &file) < 0) {
        /* File doesn't exist, create it */
        file_t* f = NULL;
        if (vfs_open("/hello.txt", O_CREAT | O_WRONLY, &f) >= 0) {
            const char* msg = "Welcome to Chanux OS!\n\n"
                              "This is a simple educational operating system.\n"
                              "Type 'help' for available commands.\n";
            vfs_write(f, msg, strlen(msg));
            vfs_close(f);
        }
    }

    /* Create a README file */
    file_t* readme = NULL;
    if (vfs_open("/README", O_CREAT | O_WRONLY, &readme) >= 0) {
        const char* msg = "=== Chanux OS ===\n\n"
                          "An educational x86_64 operating system.\n\n"
                          "Features:\n"
                          "- 64-bit long mode\n"
                          "- Virtual memory with paging\n"
                          "- Preemptive multitasking\n"
                          "- User/kernel mode separation\n"
                          "- System calls (SYSCALL/SYSRET)\n"
                          "- RAM-based filesystem\n"
                          "- Interactive shell\n";
        vfs_write(readme, msg, strlen(msg));
        vfs_close(readme);
    }

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[FS] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Filesystem ready (RAMFS mounted at /)\n");
}

/* =============================================================================
 * Interrupt Subsystem Initialization
 * =============================================================================
 */

/* =============================================================================
 * Demo Processes for Phase 4
 * =============================================================================
 */

/**
 * Demo process A - prints tick messages.
 */
static void demo_process_a(void* arg) {
    int id = (int)(uint64_t)arg;
    int count = 0;

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[Process %d] ", id);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Started!\n");

    while (1) {
        /* Print tick message every ~1 second (100 ticks) */
        for (volatile int i = 0; i < 5000000; i++) {
            /* Busy wait - consumes CPU time */
        }

        count++;
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        kprintf("[P%d] ", id);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        kprintf("tick %d\n", count);

        /* Exit after 10 ticks for demo purposes */
        if (count >= 10) {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            kprintf("[Process %d] ", id);
            vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            kprintf("Finished!\n");
            break;
        }
    }
}

/**
 * Demo process B - similar to A but with different timing.
 */
static void demo_process_b(void* arg) {
    int id = (int)(uint64_t)arg;
    int count = 0;

    vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    kprintf("[Process %d] ", id);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Started!\n");

    while (1) {
        /* Slightly different timing than process A */
        for (volatile int i = 0; i < 4000000; i++) {
            /* Busy wait */
        }

        count++;
        vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
        kprintf("[P%d] ", id);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        kprintf("tick %d\n", count);

        if (count >= 12) {
            vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
            kprintf("[Process %d] ", id);
            vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            kprintf("Finished!\n");
            break;
        }
    }
}

/* =============================================================================
 * Interrupt Subsystem Initialization
 * =============================================================================
 */

static void interrupts_init(void) {
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("[INT] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Initializing interrupt subsystem...\n");

    /* Step 1: Load GDT with TSS */
    gdt_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[INT] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("GDT with TSS loaded\n");

    /* Step 2: Initialize IRQ framework */
    irq_init();

    /* Step 3: Initialize IDT */
    idt_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[INT] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("IDT initialized with 256 entries\n");

    /* Step 4: Initialize PIC */
    pic_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[INT] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("PIC remapped: IRQ 0-15 -> vectors 32-47\n");

    /* Step 5: Initialize PIT timer */
    pit_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[INT] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("PIT configured for 100 Hz (10 ms/tick)\n");

    /* Step 6: Initialize keyboard */
    keyboard_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[INT] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("PS/2 keyboard driver initialized\n");

    /* Step 7: Enable interrupts */
    sti();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[INT] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Interrupts enabled!\n");

    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[INT] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Interrupt subsystem ready\n");
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
     * Step 3: Initialize Memory Management
     * ==========================================================================
     */
    mm_init(boot_info);

    /* ==========================================================================
     * Step 4: Initialize Interrupt Subsystem
     * ==========================================================================
     */
    interrupts_init();

    /* ==========================================================================
     * Step 5: Display System Status
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
     * Step 5: Initialize Filesystem (Phase 6)
     * ==========================================================================
     */
    fs_init();

    /* ==========================================================================
     * Step 6: Initialize Process Management (Phase 4)
     * ==========================================================================
     */
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("[PROC] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Initializing process management...\n");

    /* Initialize process subsystem (creates idle process) */
    process_init();

    /* Initialize scheduler */
    sched_init();

    /* ==========================================================================
     * Step 7: Initialize System Call Interface (Phase 5)
     * ==========================================================================
     */
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("[SYSCALL] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Initializing system call interface...\n");

    /* Initialize SYSCALL/SYSRET MSRs and handler */
    syscall_init();

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[SYSCALL] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("System call interface ready!\n");

    /* Create demo kernel processes */
    process_create("demo_a", demo_process_a, (void*)1);
    process_create("demo_b", demo_process_b, (void*)2);

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[PROC] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Process management ready!\n");

    /* ==========================================================================
     * Step 8: Create Shell User Process (Phase 6)
     * ==========================================================================
     */
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("[USER] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Creating shell user process...\n");

    /* Get the size of the embedded shell program */
    size_t shell_size = (size_t)((uintptr_t)_user_shell_end - (uintptr_t)_user_shell_start);
    kprintf("[USER] Shell program size: %d bytes\n", (int)shell_size);

    /* Create the shell user process */
    pid_t shell_pid = user_process_create("shell", _user_shell_start, shell_size);
    if (shell_pid != (pid_t)-1) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("[USER] ");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        kprintf("User process 'shell' created with PID %d\n", (int)shell_pid);
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("[USER] ");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        kprintf("Failed to create shell process!\n");
    }

    /* ==========================================================================
     * Phase 6 Complete - File System & Shell
     * ==========================================================================
     */
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("=================================================\n");
    kprintf("  Phase 6 Complete: File System & Shell\n");
    kprintf("=================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("\n");
    kprintf("Filesystem Features:\n");
    kprintf("  [x] Virtual File System (VFS) layer\n");
    kprintf("  [x] RAM-based filesystem (RAMFS) - 4MB\n");
    kprintf("  [x] Inode-based file management\n");
    kprintf("  [x] Directory support with path resolution\n");
    kprintf("  [x] Per-process file descriptor tables\n");
    kprintf("  [x] Per-process working directory (cwd)\n");
    kprintf("\n");
    kprintf("Shell Commands:\n");
    kprintf("  help    - Show available commands\n");
    kprintf("  echo    - Print arguments\n");
    kprintf("  cat     - Display file contents\n");
    kprintf("  ls      - List directory contents\n");
    kprintf("  pwd     - Print working directory\n");
    kprintf("  cd      - Change directory\n");
    kprintf("  clear   - Clear screen\n");
    kprintf("  exit    - Exit shell\n");
    kprintf("\n");
    kprintf("File System Calls: open, close, read, write,\n");
    kprintf("  lseek, stat, fstat, readdir, getcwd, chdir\n");
    kprintf("\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("Starting scheduler with interactive shell...\n\n");

    /* ==========================================================================
     * Start the Scheduler
     * ==========================================================================
     * This never returns - control transfers to the first ready process.
     */
    sched_start();

    /* Should never reach here */
    PANIC("sched_start returned!");
}
