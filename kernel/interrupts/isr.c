/**
 * =============================================================================
 * Chanux OS - Interrupt Service Routines Implementation
 * =============================================================================
 * Handles CPU exceptions (vectors 0-31).
 *
 * Exception handlers display diagnostic information and:
 *   - For recoverable exceptions: continue execution (future)
 *   - For unrecoverable exceptions: panic and halt
 *
 * The isr_handler() function is called from idt.asm after registers are saved.
 * =============================================================================
 */

#include "../include/interrupts/isr.h"
#include "../include/interrupts/idt.h"
#include "../include/kernel.h"
#include "../drivers/vga/vga.h"

/* =============================================================================
 * Exception Names
 * =============================================================================
 */

const char* exception_names[] = {
    "Divide Error",                     /* 0 */
    "Debug",                            /* 1 */
    "Non-Maskable Interrupt",           /* 2 */
    "Breakpoint",                       /* 3 */
    "Overflow",                         /* 4 */
    "Bound Range Exceeded",             /* 5 */
    "Invalid Opcode",                   /* 6 */
    "Device Not Available",             /* 7 */
    "Double Fault",                     /* 8 */
    "Coprocessor Segment Overrun",      /* 9 */
    "Invalid TSS",                      /* 10 */
    "Segment Not Present",              /* 11 */
    "Stack Segment Fault",              /* 12 */
    "General Protection Fault",         /* 13 */
    "Page Fault",                       /* 14 */
    "Reserved",                         /* 15 */
    "x87 FPU Floating-Point Error",     /* 16 */
    "Alignment Check",                  /* 17 */
    "Machine Check",                    /* 18 */
    "SIMD Floating-Point Exception",    /* 19 */
    "Virtualization Exception",         /* 20 */
    "Control Protection Exception",     /* 21 */
    "Reserved",                         /* 22 */
    "Reserved",                         /* 23 */
    "Reserved",                         /* 24 */
    "Reserved",                         /* 25 */
    "Reserved",                         /* 26 */
    "Reserved",                         /* 27 */
    "Reserved",                         /* 28 */
    "Reserved",                         /* 29 */
    "Reserved",                         /* 30 */
    "Reserved",                         /* 31 */
};

/* =============================================================================
 * Handler Table
 * =============================================================================
 */

/* Table of registered ISR handlers */
static isr_handler_t isr_handlers[IDT_ENTRIES];

/* =============================================================================
 * Exception Handlers
 * =============================================================================
 */

/**
 * Handle page fault exception.
 * Displays fault address and error code details.
 */
static void exception_page_fault(registers_t* regs) {
    uint64_t fault_addr = read_cr2();

    /* Decode error code */
    bool present = (regs->err_code & 0x01) != 0;
    bool write = (regs->err_code & 0x02) != 0;
    bool user = (regs->err_code & 0x04) != 0;
    bool reserved = (regs->err_code & 0x08) != 0;
    bool fetch = (regs->err_code & 0x10) != 0;

    cli();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n\n*** PAGE FAULT ***\n");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    kprintf("\nFault Address: 0x%p\n", (void*)fault_addr);
    kprintf("Error Code:    0x%x\n", (uint32_t)regs->err_code);
    kprintf("\nCause: ");

    if (!present) {
        kprintf("Page not present");
    } else {
        kprintf("Protection violation");
    }
    kprintf(" during %s ", write ? "write" : "read");
    if (fetch) {
        kprintf("(instruction fetch) ");
    }
    kprintf("in %s mode", user ? "user" : "kernel");
    if (reserved) {
        kprintf(" [reserved bit set]");
    }
    kprintf("\n");

    kprintf("\nRegisters:\n");
    kprintf("  RIP: 0x%p  RSP: 0x%p\n", (void*)regs->rip, (void*)regs->rsp);
    kprintf("  CS:  0x%x        SS:  0x%x\n", (uint32_t)regs->cs, (uint32_t)regs->ss);
    kprintf("  RAX: 0x%p  RBX: 0x%p\n", (void*)regs->rax, (void*)regs->rbx);
    kprintf("  RCX: 0x%p  RDX: 0x%p\n", (void*)regs->rcx, (void*)regs->rdx);

    kprintf("\nSystem halted.\n");

    for (;;) {
        halt();
    }
}

/**
 * Handle double fault exception.
 * This is always fatal - indicates a serious system problem.
 */
static void exception_double_fault(registers_t* regs) {
    cli();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n\n*** DOUBLE FAULT ***\n");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    kprintf("\nA double fault occurred!\n");
    kprintf("This usually indicates a kernel stack overflow or corrupted IDT.\n");
    kprintf("\nError Code: 0x%x\n", (uint32_t)regs->err_code);
    kprintf("RIP: 0x%p\n", (void*)regs->rip);
    kprintf("RSP: 0x%p\n", (void*)regs->rsp);

    kprintf("\nSystem halted.\n");

    for (;;) {
        halt();
    }
}

/**
 * Handle general protection fault.
 */
static void exception_gpf(registers_t* regs) {
    cli();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n\n*** GENERAL PROTECTION FAULT ***\n");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    kprintf("\nError Code: 0x%x\n", (uint32_t)regs->err_code);

    /* Decode segment selector error code if applicable */
    if (regs->err_code != 0) {
        bool external = (regs->err_code & 0x01) != 0;
        uint8_t table = (regs->err_code >> 1) & 0x03;
        uint16_t index = (regs->err_code >> 3) & 0x1FFF;

        const char* table_name;
        switch (table) {
            case 0: table_name = "GDT"; break;
            case 1: table_name = "IDT"; break;
            case 2: table_name = "LDT"; break;
            case 3: table_name = "IDT"; break;
            default: table_name = "???"; break;
        }

        kprintf("  External: %s\n", external ? "yes" : "no");
        kprintf("  Table:    %s\n", table_name);
        kprintf("  Index:    %d\n", index);
    }

    kprintf("\nRegisters:\n");
    kprintf("  RIP: 0x%p  RSP: 0x%p\n", (void*)regs->rip, (void*)regs->rsp);
    kprintf("  CS:  0x%x        SS:  0x%x\n", (uint32_t)regs->cs, (uint32_t)regs->ss);
    kprintf("  RAX: 0x%p  RBX: 0x%p\n", (void*)regs->rax, (void*)regs->rbx);

    kprintf("\nSystem halted.\n");

    for (;;) {
        halt();
    }
}

/**
 * Handle divide error.
 */
static void exception_divide_error(registers_t* regs) {
    cli();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n\n*** DIVIDE ERROR ***\n");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    kprintf("\nAttempted division by zero!\n");
    kprintf("RIP: 0x%p\n", (void*)regs->rip);

    kprintf("\nSystem halted.\n");

    for (;;) {
        halt();
    }
}

/**
 * Handle invalid opcode.
 */
static void exception_invalid_opcode(registers_t* regs) {
    cli();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n\n*** INVALID OPCODE ***\n");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    kprintf("\nAttempted to execute invalid instruction!\n");
    kprintf("RIP: 0x%p\n", (void*)regs->rip);

    kprintf("\nSystem halted.\n");

    for (;;) {
        halt();
    }
}

/**
 * Default exception handler.
 * Called for any exception without a specific handler.
 */
static void exception_default(registers_t* regs) {
    uint8_t int_no = (uint8_t)regs->int_no;

    cli();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n\n*** EXCEPTION ***\n");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    if (int_no < 32) {
        kprintf("\nException: %s (Vector %d)\n", exception_names[int_no], int_no);
    } else {
        kprintf("\nException: Unknown (Vector %d)\n", int_no);
    }

    kprintf("Error Code: 0x%x\n", (uint32_t)regs->err_code);
    kprintf("RIP: 0x%p\n", (void*)regs->rip);
    kprintf("RSP: 0x%p\n", (void*)regs->rsp);

    kprintf("\nSystem halted.\n");

    for (;;) {
        halt();
    }
}

/* =============================================================================
 * ISR Handler Registration
 * =============================================================================
 */

/**
 * Register a handler for an interrupt vector.
 */
void isr_register_handler(uint8_t vector, isr_handler_t handler) {
    isr_handlers[vector] = handler;
}

/* =============================================================================
 * Common ISR Handler
 * =============================================================================
 */

/**
 * Common ISR handler called from assembly stubs.
 * Dispatches to registered handlers or default handler.
 */
void isr_handler(registers_t* regs) {
    uint8_t int_no = (uint8_t)regs->int_no;

    /* Check for registered handler first */
    if (isr_handlers[int_no] != NULL) {
        isr_handlers[int_no](regs);
        return;
    }

    /* Use built-in handlers for known exceptions */
    switch (int_no) {
        case EXCEPTION_DE:  /* Divide Error */
            exception_divide_error(regs);
            break;

        case EXCEPTION_UD:  /* Invalid Opcode */
            exception_invalid_opcode(regs);
            break;

        case EXCEPTION_DF:  /* Double Fault */
            exception_double_fault(regs);
            break;

        case EXCEPTION_GP:  /* General Protection Fault */
            exception_gpf(regs);
            break;

        case EXCEPTION_PF:  /* Page Fault */
            exception_page_fault(regs);
            break;

        default:
            /* Unhandled exception */
            exception_default(regs);
            break;
    }
}
