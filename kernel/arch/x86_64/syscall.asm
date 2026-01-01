; =============================================================================
; Chanux OS - System Call Entry Point (x86_64 SYSCALL/SYSRET)
; =============================================================================
; This file implements the low-level syscall handler that bridges user mode
; to kernel mode using the SYSCALL/SYSRET mechanism.
;
; SYSCALL Instruction Behavior:
;   - RCX <- RIP (return address saved by CPU)
;   - R11 <- RFLAGS (flags saved by CPU)
;   - RIP <- IA32_LSTAR MSR (jumps to syscall_entry)
;   - RFLAGS <- RFLAGS AND NOT(IA32_FMASK) (clears IF)
;   - CS <- IA32_STAR[47:32] (kernel code segment)
;   - SS <- IA32_STAR[47:32] + 8 (kernel data segment)
;
; SYSRET Instruction Behavior:
;   - RIP <- RCX (return to user code)
;   - RFLAGS <- R11 (restore user flags)
;   - CS <- IA32_STAR[63:48] + 16 (user code segment)
;   - SS <- IA32_STAR[63:48] + 8 (user data segment)
;
; Calling Convention:
;   - Syscall number in RAX
;   - Arguments: RDI, RSI, RDX, R10, R8, R9 (R10 replaces RCX which is clobbered)
;   - Return value in RAX
; =============================================================================

section .data
    ; Per-CPU data (for single CPU, just global variables)
    ; These will be accessed by the syscall entry/exit code
    align 16
    global syscall_kstack_top
    syscall_kstack_top:   dq 0      ; Kernel RSP to load on syscall entry

    global user_stack_save
    user_stack_save:    dq 0        ; User RSP saved on syscall entry

section .text
    bits 64

; =============================================================================
; syscall_entry - SYSCALL Instruction Handler
; =============================================================================
; On entry (from SYSCALL instruction):
;   RCX = user RIP (return address)
;   R11 = user RFLAGS
;   RAX = syscall number
;   RDI, RSI, RDX, R10, R8, R9 = syscall arguments
;
; We need to:
;   1. Save user RSP and load kernel RSP
;   2. Save registers for return
;   3. Call C dispatcher
;   4. Restore and SYSRET back to user mode
; =============================================================================

global syscall_entry
syscall_entry:
    ; -------------------------------------------------------------------------
    ; Switch to kernel stack
    ; -------------------------------------------------------------------------
    ; Save user RSP (currently in RSP since we haven't switched yet)
    mov [rel user_stack_save], rsp

    ; Load kernel RSP from per-process storage
    ; For now, we use TSS.RSP0 which is set during context switch
    ; The kernel stack was set up when the process was created
    mov rsp, [rel syscall_kstack_top]

    ; -------------------------------------------------------------------------
    ; Build syscall frame on kernel stack
    ; -------------------------------------------------------------------------
    ; Push user RSP first (at bottom of frame)
    push qword [rel user_stack_save]

    ; Push RCX and R11 (user RIP and RFLAGS, saved by SYSCALL)
    push r11                    ; User RFLAGS
    push rcx                    ; User RIP

    ; Push callee-saved registers (we must preserve these)
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; -------------------------------------------------------------------------
    ; Enable interrupts now that we're on kernel stack
    ; -------------------------------------------------------------------------
    sti

    ; -------------------------------------------------------------------------
    ; Prepare arguments for syscall_dispatch
    ; -------------------------------------------------------------------------
    ; C calling convention: RDI, RSI, RDX, RCX, R8, R9
    ; Syscall convention:   RAX=num, RDI, RSI, RDX, R10, R8, R9
    ;
    ; We need to shuffle:
    ;   arg1 (RDI) -> RSI (second C arg)
    ;   arg2 (RSI) -> RDX (third C arg)
    ;   arg3 (RDX) -> RCX (fourth C arg)
    ;   arg4 (R10) -> R8 (fifth C arg)
    ;   arg5 (R8)  -> R9 (sixth C arg)
    ;   syscall num (RAX) -> RDI (first C arg)

    ; Save original values before shuffling
    mov r15, rdi                ; Save arg1
    mov r14, rsi                ; Save arg2
    mov r13, rdx                ; Save arg3
    mov r12, r10                ; Save arg4
    ; R8, R9 will be handled last

    ; Now set up C call arguments
    mov rdi, rax                ; First arg: syscall number
    mov rsi, r15                ; Second arg: original RDI (arg1)
    mov rdx, r14                ; Third arg: original RSI (arg2)
    mov rcx, r13                ; Fourth arg: original RDX (arg3)
    ; R8 stays as R8 (fifth C arg = arg4... wait, we need R10 here)
    ; Actually let me reconsider...

    ; Correction: syscall args are RDI, RSI, RDX, R10, R8, R9
    ; C dispatch: syscall_dispatch(num, arg1, arg2, arg3, arg4, arg5)
    ; So: RDI=num, RSI=arg1, RDX=arg2, RCX=arg3, R8=arg4, R9=arg5

    mov r8, r12                 ; Fifth arg: original R10 (arg4)
    ; R9 stays as R9 for arg5

    ; -------------------------------------------------------------------------
    ; Call C dispatcher
    ; -------------------------------------------------------------------------
    extern syscall_dispatch
    call syscall_dispatch

    ; Return value is in RAX - keep it there for return to user

    ; -------------------------------------------------------------------------
    ; Disable interrupts for return path
    ; -------------------------------------------------------------------------
    cli

    ; -------------------------------------------------------------------------
    ; Restore callee-saved registers
    ; -------------------------------------------------------------------------
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; -------------------------------------------------------------------------
    ; Restore RCX and R11 for SYSRET
    ; -------------------------------------------------------------------------
    pop rcx                     ; User RIP
    pop r11                     ; User RFLAGS

    ; Pop user RSP (but don't load it yet)
    pop rsp                     ; Restore user stack pointer

    ; -------------------------------------------------------------------------
    ; Return to user mode
    ; -------------------------------------------------------------------------
    ; SYSRET will:
    ;   - Load RIP from RCX
    ;   - Load RFLAGS from R11
    ;   - Set CS to (STAR[63:48] + 16) = 0x33 (user code)
    ;   - Set SS to (STAR[63:48] + 8) = 0x2B (user data)
    o64 sysret

; =============================================================================
; syscall_set_kernel_stack - Set kernel stack for current process
; =============================================================================
; void syscall_set_kernel_stack(uint64_t stack_top)
;
; Called during context switch to update the kernel stack pointer
; that will be used on the next syscall entry.
; =============================================================================

global syscall_set_kernel_stack
syscall_set_kernel_stack:
    mov [rel syscall_kstack_top], rdi
    ret

; =============================================================================
; syscall_get_kernel_stack - Get current kernel stack setting
; =============================================================================
; uint64_t syscall_get_kernel_stack(void)
; =============================================================================

global syscall_get_kernel_stack
syscall_get_kernel_stack:
    mov rax, [rel syscall_kstack_top]
    ret
