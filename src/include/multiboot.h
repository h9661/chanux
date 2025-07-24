/*
 * Multiboot Header Definitions
 * 
 * This file contains structures and constants defined by the Multiboot
 * specification. These structures are used to receive information from
 * the bootloader about system memory, modules, and other boot parameters.
 */

#ifndef _MULTIBOOT_H
#define _MULTIBOOT_H

#include <stdint.h>

/* Multiboot magic number passed by bootloader */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Multiboot info structure flags */
#define MULTIBOOT_INFO_MEMORY     0x00000001  /* mem_* fields are valid */
#define MULTIBOOT_INFO_BOOTDEV    0x00000002  /* boot_device field is valid */
#define MULTIBOOT_INFO_CMDLINE    0x00000004  /* cmdline field is valid */
#define MULTIBOOT_INFO_MODS       0x00000008  /* mods_* fields are valid */
#define MULTIBOOT_INFO_AOUT_SYMS  0x00000010  /* aout_sym field is valid */
#define MULTIBOOT_INFO_ELF_SHDR   0x00000020  /* elf_sec field is valid */
#define MULTIBOOT_INFO_MEM_MAP    0x00000040  /* mmap_* fields are valid */
#define MULTIBOOT_INFO_DRIVE_INFO 0x00000080  /* drives_* fields are valid */
#define MULTIBOOT_INFO_CONFIG_TAB 0x00000100  /* config_table field is valid */
#define MULTIBOOT_INFO_BOOT_NAME  0x00000200  /* boot_loader_name field is valid */
#define MULTIBOOT_INFO_APM_TABLE  0x00000400  /* apm_table field is valid */
#define MULTIBOOT_INFO_VBE_INFO   0x00000800  /* vbe_* fields are valid */

/* Multiboot memory map entry structure */
struct multiboot_mmap_entry {
    uint32_t size;      /* Size of this entry (excluding this field) */
    uint64_t addr;      /* Physical address */
    uint64_t len;       /* Length in bytes */
    uint32_t type;      /* Memory type */
} __attribute__((packed));

/* Memory types for multiboot_mmap_entry */
#define MULTIBOOT_MEMORY_AVAILABLE        1  /* Memory available for use */
#define MULTIBOOT_MEMORY_RESERVED         2  /* Reserved memory (do not use) */
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3  /* ACPI data, can be reclaimed */
#define MULTIBOOT_MEMORY_NVS              4  /* ACPI NVS memory */
#define MULTIBOOT_MEMORY_BADRAM           5  /* Defective RAM */

/* Main multiboot information structure */
struct multiboot_info {
    /* Multiboot info version number */
    uint32_t flags;
    
    /* Available memory from BIOS */
    uint32_t mem_lower;    /* Amount of lower memory (in KB) */
    uint32_t mem_upper;    /* Amount of upper memory (in KB) */
    
    /* "root" partition */
    uint32_t boot_device;
    
    /* Kernel command line */
    uint32_t cmdline;
    
    /* Boot-Module list */
    uint32_t mods_count;
    uint32_t mods_addr;
    
    /* Symbol table for kernel in a.out format */
    union {
        struct {
            uint32_t tabsize;
            uint32_t strsize;
            uint32_t addr;
            uint32_t reserved;
        } aout_sym;
        
        /* Section header table for kernel in ELF format */
        struct {
            uint32_t num;
            uint32_t size;
            uint32_t addr;
            uint32_t shndx;
        } elf_sec;
    } u;
    
    /* Memory Mapping buffer */
    uint32_t mmap_length;
    uint32_t mmap_addr;
    
    /* Drive Info buffer */
    uint32_t drives_length;
    uint32_t drives_addr;
    
    /* ROM configuration table */
    uint32_t config_table;
    
    /* Boot Loader Name */
    uint32_t boot_loader_name;
    
    /* APM table */
    uint32_t apm_table;
    
    /* Video information */
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} __attribute__((packed));

/* Helper macros for checking multiboot flags */
#define CHECK_FLAG(flags, bit) ((flags) & (1 << (bit)))

#endif /* _MULTIBOOT_H */