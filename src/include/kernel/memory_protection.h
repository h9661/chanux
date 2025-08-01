/*
 * Memory Protection Header
 * 
 * This file defines enhanced memory protection mechanisms for user mode support,
 * including proper kernel/user separation, access validation, and security features.
 */

#ifndef _MEMORY_PROTECTION_H
#define _MEMORY_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "kernel/paging.h"

/* Memory space boundaries */
#define KERNEL_SPACE_START    0x00000000
#define KERNEL_SPACE_END      0xC0000000  /* 3GB mark */

/* Redefine user space boundaries to match our 1GB constraint */
#ifdef USER_SPACE_START
#undef USER_SPACE_START
#endif
#define USER_SPACE_START      0x08048000  /* Standard ELF load address */

#ifdef USER_SPACE_END
#undef USER_SPACE_END
#endif
#define USER_SPACE_END        0x3FFFF000  /* User space ends at ~1GB */

/* Enhanced page flags for protection */
#define PAGE_COW        0x200    /* Copy-on-write page (available bit 9) */
#define PAGE_SHARED     0x400    /* Shared between processes (available bit 10) */
#define PAGE_LOCKED     0x800    /* Page locked in memory (available bit 11) */

/* Protection flag combinations */
#define PAGE_KERNEL_RO  (PAGE_PRESENT)
#define PAGE_KERNEL_RW  (PAGE_PRESENT | PAGE_WRITABLE)
#define PAGE_USER_RO    (PAGE_PRESENT | PAGE_USER)
#define PAGE_USER_RW    (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)

/* Security violation types */
typedef enum {
    SECURITY_OK = 0,
    SECURITY_VIOLATION_KERNEL_ACCESS,    /* User tried to access kernel memory */
    SECURITY_VIOLATION_PRIVILEGE,        /* Privilege level violation */
    SECURITY_VIOLATION_EXECUTE,          /* Execute from non-executable page */
    SECURITY_VIOLATION_STACK_OVERFLOW,   /* Stack overflow detected */
    SECURITY_VIOLATION_INVALID_POINTER   /* Invalid user pointer in syscall */
} security_violation_t;

/* Page fault error information */
typedef struct {
    bool present;           /* Page present in memory */
    bool write;            /* Write access attempted */
    bool user_mode;        /* Fault from user mode */
    bool reserved_write;   /* Reserved bit set */
    bool instruction_fetch;/* Instruction fetch */
} fault_error_t;

/* Memory access validation functions */
static inline bool is_user_address(void *addr) {
    uint32_t uaddr = (uint32_t)addr;
    return uaddr >= USER_SPACE_START && uaddr < USER_SPACE_END;
}

static inline bool is_kernel_address(void *addr) {
    uint32_t kaddr = (uint32_t)addr;
    return kaddr < KERNEL_SPACE_END;
}

static inline bool is_user_range(void *addr, size_t size) {
    uint32_t start = (uint32_t)addr;
    uint32_t end = start + size;
    
    /* Check for overflow */
    if (end < start)
        return false;
        
    return start >= USER_SPACE_START && end <= USER_SPACE_END;
}

/* Function prototypes for security checks */
bool validate_user_pointer(const void *ptr, size_t size, int access);
int copy_from_user(void *kernel_dest, const void *user_src, size_t n);
int copy_to_user(void *user_dest, const void *kernel_src, size_t n);
int strncpy_from_user(char *dest, const char *src, size_t n);
security_violation_t check_memory_access(uint32_t addr, fault_error_t error, bool kernel_mode);
void handle_security_violation(uint32_t fault_addr, security_violation_t violation);

/* Enhanced page table entry helpers */
static inline bool is_page_user_accessible(page_table_entry_t entry) {
    return (entry & PAGE_PRESENT) && (entry & PAGE_USER);
}

static inline bool is_page_writable_by_user(page_table_entry_t entry) {
    return is_page_user_accessible(entry) && (entry & PAGE_WRITABLE);
}

static inline void set_page_protection(page_table_entry_t *entry, uint32_t flags) {
    *entry = (*entry & ~0xFFF) | (flags & 0xFFF);
}

#endif /* _MEMORY_PROTECTION_H */