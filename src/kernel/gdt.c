/*
 * Global Descriptor Table (GDT) Implementation
 * 
 * The GDT is a data structure used by Intel x86-family processors to define
 * the characteristics of the various memory areas (segments) used during
 * program execution, including the base address, size, and access privileges.
 */

#include <stdint.h>

/* GDT Entry Structure
 * Each entry is 8 bytes and defines a memory segment
 * The fields are arranged in a specific way required by the x86 architecture
 */
struct gdt_entry {
    uint16_t limit_low;     /* Lower 16 bits of segment limit */
    uint16_t base_low;      /* Lower 16 bits of base address */
    uint8_t base_middle;    /* Middle 8 bits of base address */
    uint8_t access;         /* Access flags (privilege level, type, etc.) */
    uint8_t granularity;    /* Granularity and upper 4 bits of limit */
    uint8_t base_high;      /* Upper 8 bits of base address */
} __attribute__((packed));  /* Prevent compiler from adding padding */

/* GDT Pointer Structure
 * This structure is loaded into the GDTR register using LGDT instruction
 */
struct gdt_ptr {
    uint16_t limit;         /* Size of GDT minus 1 */
    uint32_t base;          /* Linear address of GDT */
} __attribute__((packed));

/* Our GDT with 6 entries (including TSS) */
struct gdt_entry gdt[6];
struct gdt_ptr gp;

/* External assembly function to load the GDT */
extern void gdt_flush(uint32_t);

/*
 * gdt_set_gate - Set up a GDT entry
 * @num: Entry number in the GDT
 * @base: Starting address of the segment
 * @limit: Size of the segment (in bytes or 4KB pages)
 * @access: Access byte containing privilege and type information
 * @gran: Granularity byte containing size information and flags
 *
 * Access byte format:
 * Bit 7: Present bit (must be 1 for valid segments)
 * Bits 5-6: Privilege level (0=kernel, 3=user)
 * Bit 4: Descriptor type (1=code/data, 0=system)
 * Bits 0-3: Segment type (code/data, read/write, etc.)
 *
 * Granularity byte format:
 * Bit 7: Granularity (0=byte, 1=4KB page)
 * Bit 6: Size (0=16-bit, 1=32-bit)
 * Bits 4-5: Always 0
 * Bits 0-3: Upper 4 bits of limit
 */
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    /* Set base address (split across three fields) */
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    
    /* Set limit (split across two fields) */
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);
    
    /* Set granularity flags and access byte */
    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

/*
 * gdt_install - Initialize and install the GDT
 *
 * Sets up the Global Descriptor Table with the following segments:
 * 0: Null segment (required by x86 architecture)
 * 1: Kernel code segment
 * 2: Kernel data segment
 * 3: User code segment
 * 4: User data segment
 * 5: TSS segment (added later)
 */
void gdt_install() {
    /* Set up GDT pointer */
    gp.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gp.base = (uint32_t)&gdt;
    
    /* Null segment - required by x86 architecture */
    gdt_set_gate(0, 0, 0, 0, 0);
    
    /* Kernel code segment
     * Base: 0, Limit: 4GB, Access: 0x9A (present, ring 0, code segment, readable)
     * Granularity: 0xCF (4KB pages, 32-bit mode)
     */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    /* Kernel data segment
     * Base: 0, Limit: 4GB, Access: 0x92 (present, ring 0, data segment, writable)
     * Granularity: 0xCF (4KB pages, 32-bit mode)
     */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    /* User code segment
     * Base: 0, Limit: 4GB, Access: 0xFA (present, ring 3, code segment, readable)
     * Granularity: 0xCF (4KB pages, 32-bit mode)
     */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    
    /* User data segment
     * Base: 0, Limit: 4GB, Access: 0xF2 (present, ring 3, data segment, writable)
     * Granularity: 0xCF (4KB pages, 32-bit mode)
     */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    
    /* Load the GDT into the GDTR register */
    gdt_flush((uint32_t)&gp);
}