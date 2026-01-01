; =============================================================================
; Chanux OS - User Space Syscall Assembly
; =============================================================================
; This file provides the low-level syscall interface for user programs.
;
; The SYSCALL instruction uses the following register convention:
;   RAX = syscall number
;   RDI = arg1
;   RSI = arg2
;   RDX = arg3
;   R10 = arg4 (RCX is used by SYSCALL instruction itself)
;   R8  = arg5
;   R9  = arg6 (not used in our current implementation)
;
; On return:
;   RAX = return value
;   RCX = destroyed (contains saved RIP)
;   R11 = destroyed (contains saved RFLAGS)
;
; All other registers are preserved by the kernel.
; =============================================================================

[BITS 64]

section .text

; =============================================================================
; syscall_raw(num, arg1, arg2, arg3, arg4, arg5)
; =============================================================================
; Low-level syscall invocation.
;
; Arguments (System V AMD64 ABI):
;   RDI = syscall number
;   RSI = arg1
;   RDX = arg2
;   RCX = arg3
;   R8  = arg4
;   R9  = arg5
;
; We need to rearrange to match SYSCALL convention:
;   RAX = syscall number (from RDI)
;   RDI = arg1 (from RSI)
;   RSI = arg2 (from RDX)
;   RDX = arg3 (from RCX)
;   R10 = arg4 (from R8, because RCX is clobbered by SYSCALL)
;   R8  = arg5 (from R9)

global syscall_raw
syscall_raw:
    ; Move syscall number to RAX
    mov rax, rdi

    ; Shift arguments down
    mov rdi, rsi        ; arg1
    mov rsi, rdx        ; arg2
    mov rdx, rcx        ; arg3
    mov r10, r8         ; arg4 (into R10, not RCX)
    mov r8, r9          ; arg5

    ; Invoke kernel
    syscall

    ; Return value is already in RAX
    ret

; =============================================================================
; Helper functions for common syscalls
; =============================================================================

; void _exit(int code)
; Wrapper that never returns
global _exit
_exit:
    mov rax, 0          ; SYS_EXIT
    ; RDI already contains exit code
    syscall
    ; Should never reach here, but loop forever just in case
.loop:
    hlt
    jmp .loop
