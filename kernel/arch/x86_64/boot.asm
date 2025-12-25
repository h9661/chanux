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

    ; Set up kernel stack
    mov rsp, kernel_stack_top

    ; Clear base pointer
    xor rbp, rbp

    ; Clear BSS section
    ; RDI already contains boot info pointer, save it
    push rdi

    extern __bss_start
    extern __bss_end

    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor al, al
    rep stosb

    ; Restore boot info pointer
    pop rdi

    ; Call kernel main
    ; First argument (RDI) = boot info pointer (already set)
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
