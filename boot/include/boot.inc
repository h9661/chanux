; =============================================================================
; Chanux OS - Boot Constants and Macros
; =============================================================================
; Shared definitions used by bootloader stages
; =============================================================================

; =============================================================================
; Memory Addresses
; =============================================================================
STAGE1_ADDR         equ 0x7C00      ; Where BIOS loads Stage 1
STAGE2_ADDR         equ 0x7E00      ; Where Stage 1 loads Stage 2
KERNEL_LOAD_ADDR    equ 0x100000    ; 1MB - where we load the kernel (physical)
KERNEL_STACK_ADDR   equ 0x90000     ; Temporary stack for protected mode

; Higher-half kernel virtual address
KERNEL_VIRT_BASE    equ 0xFFFFFFFF80000000

; =============================================================================
; Memory Map Storage
; =============================================================================
; Note: Stage 2 is at 0x7E00-0xBE00 (16KB), so we must place this after Stage 2
MEMORY_MAP_ADDR     equ 0xC000      ; Where we store E820 memory map (after Stage 2)
MEMORY_MAP_ENTRIES  equ 0xC004      ; Number of entries (stored before map)

; =============================================================================
; Page Table Addresses (must be 4KB aligned)
; =============================================================================
PML4_ADDR           equ 0x1000      ; Page Map Level 4
PDPT_ADDR           equ 0x2000      ; Page Directory Pointer Table
PD_ADDR             equ 0x3000      ; Page Directory
PT_ADDR             equ 0x4000      ; Page Table (for fine-grained mapping)

; =============================================================================
; Disk Constants
; =============================================================================
KERNEL_START_SECTOR equ 34          ; Kernel starts after Stage 2 (sector 34)
KERNEL_SECTORS      equ 128         ; Number of sectors for kernel (64KB)

; =============================================================================
; GDT Segment Selectors
; =============================================================================
; 32-bit Protected Mode GDT
GDT32_NULL          equ 0x00
GDT32_CODE          equ 0x08        ; 32-bit code segment
GDT32_DATA          equ 0x10        ; 32-bit data segment

; 64-bit Long Mode GDT
GDT64_NULL          equ 0x00
GDT64_CODE          equ 0x08        ; 64-bit code segment
GDT64_DATA          equ 0x10        ; 64-bit data segment

; =============================================================================
; Control Register Bits
; =============================================================================
CR0_PE              equ (1 << 0)    ; Protection Enable
CR0_PG              equ (1 << 31)   ; Paging Enable

CR4_PAE             equ (1 << 5)    ; Physical Address Extension
CR4_PGE             equ (1 << 7)    ; Page Global Enable

; =============================================================================
; EFER MSR (Extended Feature Enable Register)
; =============================================================================
EFER_MSR            equ 0xC0000080
EFER_LME            equ (1 << 8)    ; Long Mode Enable
EFER_LMA            equ (1 << 10)   ; Long Mode Active
EFER_NXE            equ (1 << 11)   ; No-Execute Enable

; =============================================================================
; Page Table Entry Flags
; =============================================================================
PAGE_PRESENT        equ (1 << 0)    ; Page is present
PAGE_WRITABLE       equ (1 << 1)    ; Page is writable
PAGE_USER           equ (1 << 2)    ; User-accessible
PAGE_HUGE           equ (1 << 7)    ; 2MB/1GB huge page

; Common page flags
PAGE_FLAGS          equ (PAGE_PRESENT | PAGE_WRITABLE)
PAGE_FLAGS_HUGE     equ (PAGE_FLAGS | PAGE_HUGE)

; =============================================================================
; VGA Text Mode
; =============================================================================
VGA_MEMORY          equ 0xB8000
VGA_WIDTH           equ 80
VGA_HEIGHT          equ 25
VGA_WHITE_ON_BLACK  equ 0x0F

; =============================================================================
; Macros
; =============================================================================

; Print a single character in real mode
%macro PRINT_CHAR 1
    mov al, %1
    mov ah, 0x0E
    int 0x10
%endmacro

; Print newline in real mode
%macro PRINT_NEWLINE 0
    PRINT_CHAR 0x0D
    PRINT_CHAR 0x0A
%endmacro
