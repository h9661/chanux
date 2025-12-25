/**
 * =============================================================================
 * Chanux OS - Main Kernel Header
 * =============================================================================
 * Central header file that includes common kernel definitions.
 * =============================================================================
 */

#ifndef CHANUX_KERNEL_H
#define CHANUX_KERNEL_H

#include "types.h"

/* =============================================================================
 * Kernel Version
 * =============================================================================
 */

#define CHANUX_VERSION_MAJOR    0
#define CHANUX_VERSION_MINOR    1
#define CHANUX_VERSION_PATCH    0
#define CHANUX_VERSION_STRING   "0.1.0"

/* =============================================================================
 * Memory Constants
 * =============================================================================
 */

#define PAGE_SIZE           4096
#define KERNEL_PHYS_BASE    0x100000            /* 1MB */
#define KERNEL_VIRT_BASE    0xFFFFFFFF80000000  /* Higher half */

/* =============================================================================
 * Boot Information Structure
 * =============================================================================
 * Passed from bootloader to kernel
 */

/* Memory map entry from E820 */
typedef struct {
    uint64_t base;          /* Base address */
    uint64_t length;        /* Length in bytes */
    uint32_t type;          /* Type: 1=usable, 2=reserved, 3=ACPI, etc. */
    uint32_t attributes;    /* Extended attributes (ACPI 3.0) */
} PACKED memory_map_entry_t;

/* Memory types */
#define MEMORY_TYPE_USABLE          1
#define MEMORY_TYPE_RESERVED        2
#define MEMORY_TYPE_ACPI_RECLAIMABLE 3
#define MEMORY_TYPE_ACPI_NVS        4
#define MEMORY_TYPE_BAD             5

/* Boot info structure */
typedef struct {
    uint32_t memory_map_entries;
    memory_map_entry_t memory_map[32];  /* Up to 32 entries */
} PACKED boot_info_t;

/* =============================================================================
 * Kernel Panic
 * =============================================================================
 */

NORETURN void kernel_panic(const char* file, int line, const char* msg);

#define PANIC(msg) kernel_panic(__FILE__, __LINE__, msg)

/* =============================================================================
 * Assertion
 * =============================================================================
 */

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            PANIC("Assertion failed: " #condition); \
        } \
    } while (0)

/* =============================================================================
 * Inline Assembly Helpers
 * =============================================================================
 */

/* Halt the CPU */
static inline void halt(void) {
    __asm__ volatile ("hlt");
}

/* Disable interrupts */
static inline void cli(void) {
    __asm__ volatile ("cli");
}

/* Enable interrupts */
static inline void sti(void) {
    __asm__ volatile ("sti");
}

/* Read from I/O port (byte) */
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* Write to I/O port (byte) */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* Read from I/O port (word) */
static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* Write to I/O port (word) */
static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

/* I/O wait (small delay) */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* Read CR0 */
static inline uint64_t read_cr0(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

/* Read CR3 */
static inline uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

/* Write CR3 */
static inline void write_cr3(uint64_t value) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(value));
}

#endif /* CHANUX_KERNEL_H */
