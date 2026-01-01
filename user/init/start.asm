; =============================================================================
; Chanux OS - User Program Startup Code
; =============================================================================
; This is the entry point for user programs.
; It sets up the C runtime environment and calls main().
;
; When a user process starts via IRETQ:
;   - All registers are zeroed (for security)
;   - RIP points to _start
;   - RSP points to user stack
;   - We're running in Ring 3
; =============================================================================

[BITS 64]

section .text.entry

; External C entry point
extern main

; =============================================================================
; _start - User program entry point
; =============================================================================
; This is where user execution begins after IRETQ.
; We could zero BSS here, but the kernel already zeros allocated pages.

global _start
_start:
    ; Align stack to 16 bytes (required by System V ABI)
    and rsp, ~0xF

    ; Clear frame pointer for clean stack traces
    xor rbp, rbp

    ; Call the C main function
    ; Note: main() takes no arguments in our simple implementation
    call main

    ; If main returns, exit with its return value
    ; RAX contains the return value from main
    mov rdi, rax
    mov rax, 0          ; SYS_EXIT = 0
    syscall

    ; Should never reach here
.hang:
    hlt
    jmp .hang
