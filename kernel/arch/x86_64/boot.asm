; =============================================================================
; Chanux OS - Kernel Entry Point (64-bit)
; =============================================================================
; This is the assembly entry point for the kernel.
; Called from the Stage 2 bootloader after switching to long mode.
;
; Entry conditions:
;   - 64-bit long mode active
;   - Paging enabled (identity mapped + higher half)
;   - RDI = pointer to boot info (memory map)
;   - Interrupts disabled
; =============================================================================

[BITS 64]

section .text

; External C function
extern kernel_main

; Exported symbols
global _start
global kernel_stack_top

; =============================================================================
; Kernel Entry Point
; =============================================================================
_start:
    ; Clear direction flag (required by System V ABI)
    cld

    ; IMPORTANT: Save boot info pointer in callee-saved register BEFORE touching BSS
    ; RDI contains boot info pointer from bootloader
    ; We must save it before clearing BSS since our stack is in BSS!
    mov r15, rdi

    ; Clear BSS section FIRST (before setting up stack, since stack is in BSS)
    extern __bss_start
    extern __bss_end

    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor al, al
    rep stosb

    ; NOW set up kernel stack (after BSS is cleared)
    mov rsp, kernel_stack_top

    ; Clear base pointer
    xor rbp, rbp

    ; Restore boot info pointer to RDI for kernel_main argument
    mov rdi, r15

    ; Call kernel main
    ; First argument (RDI) = boot info pointer
    call kernel_main

    ; If kernel_main returns (it shouldn't), halt
.halt:
    cli
    hlt
    jmp .halt

; =============================================================================
; Kernel Stack
; =============================================================================
section .bss
align 16

kernel_stack_bottom:
    resb 16384              ; 16 KB kernel stack
kernel_stack_top:
