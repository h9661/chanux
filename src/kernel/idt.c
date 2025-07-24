/*
 * Interrupt Descriptor Table (IDT) Implementation
 *
 * The IDT is used by the x86 architecture to determine the proper response
 * to interrupts and exceptions. Each entry points to an interrupt handler.
 */

#include <stdint.h>
#include "../include/string.h"

/* IDT Entry Structure
 * Each entry is 8 bytes and defines an interrupt handler
 * When an interrupt occurs, the CPU uses this entry to find the handler code
 */
struct idt_entry {
    uint16_t base_lo;    /* Lower 16 bits of handler function address */
    uint16_t sel;        /* Kernel segment selector (usually 0x08 for code segment) */
    uint8_t always0;     /* This must always be zero */
    uint8_t flags;       /* Type and attributes (see below) */
    uint16_t base_hi;    /* Upper 16 bits of handler function address */
} __attribute__((packed));  /* Prevent compiler padding */

/* IDT Pointer Structure
 * This structure is loaded into the IDTR register using LIDT instruction
 */
struct idt_ptr {
    uint16_t limit;      /* Size of IDT minus 1 */
    uint32_t base;       /* Linear address of IDT */
} __attribute__((packed));

/* The actual IDT with 256 entries
 * 0-31: CPU exceptions (divide by zero, page fault, etc.)
 * 32-255: Available for hardware/software interrupts
 */
struct idt_entry idt[256];
struct idt_ptr idtp;

/* External assembly function to load the IDT */
extern void idt_load(uint32_t);

/*
 * idt_set_gate - Set up an IDT entry
 * @num: Interrupt number (0-255)
 * @base: Address of the interrupt handler function
 * @sel: Code segment selector (usually 0x08 for kernel code)
 * @flags: Type and attribute flags
 *
 * Flags byte format:
 * Bit 7: Present bit (must be 1 for active interrupts)
 * Bits 5-6: Privilege level (0=kernel only, 3=user callable)
 * Bit 4: Set to 0 for interrupt gates
 * Bits 0-3: Gate type (0xE = 32-bit interrupt gate, 0xF = 32-bit trap gate)
 *
 * The function automatically sets bits 5-6 to 0 (kernel privilege)
 * by OR'ing with 0x60
 */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    /* Set the base address (split into two 16-bit values) */
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    
    /* Set selector and flags */
    idt[num].sel = sel;
    idt[num].always0 = 0;
    /* OR with 0x60 to set bits 5-6 (making DPL=0, kernel only) */
    idt[num].flags = flags | 0x60;
}

/*
 * idt_install - Initialize and install the IDT
 *
 * This function sets up an empty IDT. Interrupt handlers need to be
 * added separately using idt_set_gate() for each interrupt you want
 * to handle.
 */
void idt_install() {
    /* Set up IDT pointer */
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;
    
    /* Clear the entire IDT, setting all entries to 0
     * This ensures unhandled interrupts will cause a fault
     * rather than jumping to random memory
     */
    memset(&idt, 0, sizeof(struct idt_entry) * 256);
    
    /* Load the IDT into the IDTR register
     * After this, the CPU will use our IDT for interrupt handling
     */
    idt_load((uint32_t)&idtp);
}