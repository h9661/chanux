; =============================================================================
; Chanux OS - Shell Startup Code
; =============================================================================
; Entry point for the shell program.
; Sets up C runtime environment and calls shell_main().
; =============================================================================

[BITS 64]

section .text.entry

; External C entry point
extern shell_main

; =============================================================================
; _start - Shell entry point
; =============================================================================

global _start
_start:
    ; Align stack to 16 bytes (required by System V ABI)
    and rsp, ~0xF

    ; Clear frame pointer for clean stack traces
    xor rbp, rbp

    ; Call the C shell_main function
    call shell_main

    ; If shell_main returns, exit with its return value
    mov rdi, rax
    mov rax, 0          ; SYS_EXIT = 0
    syscall

    ; Should never reach here
.hang:
    hlt
    jmp .hang
