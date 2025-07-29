; ChanUX Bootloader
; This is a simple multiboot-compliant bootloader that follows the Multiboot specification
; to allow GRUB and other bootloaders to load our kernel

; Multiboot header constants
; These flags tell the bootloader what features we need
MBOOT_PAGE_ALIGN    equ 1<<0    ; Align loaded modules on page boundaries (4KB)
MBOOT_MEM_INFO      equ 1<<1    ; Provide memory map information
MBOOT_HEADER_MAGIC  equ 0x1BADB002  ; Magic number that identifies this as a Multiboot header
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO  ; Combined flags we want
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)  ; Checksum to prove header validity

; Tell assembler to generate 32-bit code (protected mode)
[BITS 32]

; Multiboot header section - must be within first 8KB of kernel file
section .multiboot
align 4
    dd MBOOT_HEADER_MAGIC    ; Magic number for identification
    dd MBOOT_HEADER_FLAGS    ; Flags telling bootloader what we need
    dd MBOOT_CHECKSUM        ; Checksum - when added to above, must equal 0

; BSS section - uninitialized data (zeroed by bootloader)
section .bss
align 16
stack_bottom:
    resb 16384 ; Reserve 16 KiB for kernel stack
stack_top:     ; Stack grows downward, so top is at higher address

; Text section - actual code
section .text
global _start      ; Make _start symbol visible to linker
extern kernel_main ; kernel_main is defined in kernel.c

; Entry point - bootloader jumps here after loading kernel
_start:
    ; Set up the stack pointer to point to top of our stack
    ; ESP (stack pointer) now points to valid stack memory
    mov esp, stack_top
    
    ; Reset EFLAGS register to known state
    ; This clears all CPU flags (carry, zero, etc.)
    push 0
    popf
    
    ; Push multiboot parameters for kernel_main
    ; EBX contains pointer to multiboot info structure
    ; EAX contains multiboot magic number (0x2BADB002)
    push ebx
    push eax
    
    ; Call our C kernel entry point
    ; kernel_main(magic, addr) will handle the rest
    call kernel_main
    
    ; If kernel_main returns (it shouldn't), halt the CPU
    cli        ; Clear interrupt flag - disable interrupts
.hang:
    hlt        ; Halt instruction - stops CPU until next interrupt
    jmp .hang  ; If CPU wakes up somehow, halt again