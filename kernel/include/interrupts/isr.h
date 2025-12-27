/**
 * =============================================================================
 * Chanux OS - Interrupt Service Routines
 * =============================================================================
 * Defines the CPU register structure and ISR handler interface.
 *
 * When an interrupt occurs:
 *   1. CPU automatically pushes SS, RSP, RFLAGS, CS, RIP (and error code)
 *   2. ISR stub pushes interrupt number and error code (if not CPU-pushed)
 *   3. ISR stub saves all general-purpose registers
 *   4. ISR stub calls C handler with pointer to registers_t
 *   5. C handler processes the interrupt
 *   6. ISR stub restores registers and executes IRETQ
 * =============================================================================
 */

#ifndef CHANUX_ISR_H
#define CHANUX_ISR_H

#include "../types.h"

/* =============================================================================
 * CPU Register Structure
 * =============================================================================
 * This structure matches the stack layout after ISR stub saves all registers.
 * The order is critical - it must match the push order in idt.asm!
 */

typedef struct {
    /* Pushed by ISR stub (in reverse order of struct) */
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    /* Pushed by ISR stub */
    uint64_t int_no;            /* Interrupt number */
    uint64_t err_code;          /* Error code (or 0 if none) */

    /* Pushed automatically by CPU */
    uint64_t rip;               /* Instruction pointer */
    uint64_t cs;                /* Code segment */
    uint64_t rflags;            /* CPU flags */
    uint64_t rsp;               /* Stack pointer */
    uint64_t ss;                /* Stack segment */
} PACKED registers_t;

/* =============================================================================
 * ISR Handler Type
 * =============================================================================
 */

/**
 * Interrupt/Exception handler function type.
 * Called by the common ISR stub with register state.
 *
 * @param regs Pointer to saved register state
 */
typedef void (*isr_handler_t)(registers_t* regs);

/* =============================================================================
 * Exception Names (for debugging)
 * =============================================================================
 */

extern const char* exception_names[];

/* =============================================================================
 * ISR Functions
 * =============================================================================
 */

/**
 * Register a handler for an interrupt vector.
 *
 * @param vector  Interrupt vector number (0-255)
 * @param handler Handler function to call
 */
void isr_register_handler(uint8_t vector, isr_handler_t handler);

/**
 * Common ISR handler (called from assembly stubs).
 * Dispatches to registered handlers or default handler.
 *
 * @param regs Pointer to saved register state
 */
void isr_handler(registers_t* regs);

/* =============================================================================
 * ISR Stubs (defined in idt.asm)
 * =============================================================================
 * External references to assembly ISR stubs.
 */

/* Exception ISRs (0-31) */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

/* IRQ ISRs (32-47) */
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

/* ISR stub table (array of function pointers) */
extern uint64_t isr_stub_table[];

#endif /* CHANUX_ISR_H */
