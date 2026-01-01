; =============================================================================
; Chanux OS - Context Switch Assembly Routines
; =============================================================================
; Implements low-level context switching between kernel and user processes.
;
; Context switch strategy:
;   - Save callee-saved registers (System V AMD64 ABI)
;   - Save current RSP to old process's PCB
;   - Update TSS.RSP0 for new process
;   - Switch CR3 if new process has different address space
;   - Load new RSP
;   - Restore callee-saved registers from new stack
;   - "Return" to new process
;
; The stack setup for each process ensures that when context_switch returns,
; it returns to the process's saved instruction pointer.
; =============================================================================

[BITS 64]

; =============================================================================
; Debug Configuration
; =============================================================================
; DEBUG_CONTEXT is passed from Makefile via -DDEBUG_CONTEXT=1

%ifndef DEBUG_CONTEXT
    %define DEBUG_CONTEXT 0
%endif

; =============================================================================
; Debug Serial Output Macro
; =============================================================================
; Outputs a single character to serial port 0x3F8 for debugging.
; Only emits code when DEBUG_CONTEXT is enabled.

%macro DEBUG_SERIAL_CHAR 1
%if DEBUG_CONTEXT
    push rcx
    push rsi
    push rdx
    mov dx, 0x3FD           ; Line Status Register
%%wait_serial:
    in al, dx
    test al, 0x20           ; Check if TX buffer empty
    jz %%wait_serial
    mov dx, 0x3F8           ; Data register
    mov al, %1              ; Character to print
    out dx, al
    pop rdx
    pop rsi
    pop rcx
%endif
%endmacro

section .text

; External C function for updating TSS.RSP0
extern gdt_set_rsp0
; External function for updating syscall kernel stack
extern syscall_set_kernel_stack

; =============================================================================
; context_switch(old_rsp_ptr, new_rsp, new_rsp0, new_cr3)
; =============================================================================
; Performs a context switch between two processes.
;
; Parameters (System V AMD64 ABI):
;   RDI = old_rsp_ptr  - Pointer to save current RSP (uint64_t* in old PCB)
;   RSI = new_rsp      - RSP value to load for new process
;   RDX = new_rsp0     - RSP0 value for TSS (kernel stack top of new process)
;   RCX = new_cr3      - CR3 value for new process (0 = don't switch)
;
; This function:
;   1. Saves callee-saved registers (rbx, rbp, r12-r15) on current stack
;   2. Saves current RSP to *old_rsp_ptr (old process's PCB.rsp)
;   3. Updates TSS.RSP0 with new_rsp0
;   4. Switches CR3 if new_cr3 != 0 and differs from current
;   5. Loads new RSP (switches to new process's stack)
;   6. Restores callee-saved registers from new stack
;   7. Returns (to whatever address is at top of new stack - the new process)
;
; Note: This is NOT an interrupt context switch. We only save/restore the
; callee-saved registers because the caller-saved registers are already
; saved by the calling convention.

global context_switch
context_switch:
    ; =========================================================================
    ; Step 1: Save callee-saved registers on current (old) stack
    ; =========================================================================
    ; Order must match what we pop at the end!
    push r15
    push r14
    push r13
    push r12
    push rbp
    push rbx

    ; =========================================================================
    ; Step 2: Save current RSP to old process's PCB
    ; =========================================================================
    ; RDI points to old_process->rsp
    mov [rdi], rsp

    ; =========================================================================
    ; Step 3: Update TSS.RSP0 and syscall kernel stack for new process
    ; =========================================================================
    ; Need to call gdt_set_rsp0(new_rsp0) and syscall_set_kernel_stack(new_rsp0)
    ; RDX contains new_rsp0, but these functions expect argument in RDI
    ; Save all needed registers first
    push rsi                    ; Save new_rsp
    push rcx                    ; Save new_cr3
    push rdx                    ; Save new_rsp0 for second call

    mov rdi, rdx                ; new_rsp0 as first argument
    call gdt_set_rsp0

    pop rdi                     ; Restore new_rsp0 as argument
    push rdi                    ; Save it again for potential future use
    call syscall_set_kernel_stack

    add rsp, 8                  ; Pop the saved new_rsp0
    pop rcx                     ; Restore new_cr3
    pop rsi                     ; Restore new_rsp

    ; Debug: Print '1' after gdt_set_rsp0
    DEBUG_SERIAL_CHAR '1'

    ; =========================================================================
    ; Step 4: Switch CR3 if needed (for user process address space)
    ; =========================================================================
    ; RCX contains new_cr3
    ; If new_cr3 == 0, skip (kernel process uses current CR3)
    test rcx, rcx
    jz .skip_cr3_switch

    ; Debug: Print '2' before CR3 switch
    DEBUG_SERIAL_CHAR '2'

    ; Check if CR3 is actually different
    mov rax, cr3
    cmp rax, rcx
    je .skip_cr3_switch

    ; Switch to new address space
    mov cr3, rcx

    ; Debug: Print '3' after CR3 switch
    DEBUG_SERIAL_CHAR '3'

.skip_cr3_switch:
    ; =========================================================================
    ; Step 5: Switch to new process's stack
    ; =========================================================================
    mov rsp, rsi

    ; Debug: Print 'X' marker after stack switch
    DEBUG_SERIAL_CHAR 'X'

    ; =========================================================================
    ; Step 6: Restore callee-saved registers from new stack
    ; =========================================================================
    ; Order must match what we pushed in step 1!
    pop rbx
    pop rbp
    pop r12
    pop r13
    pop r14
    pop r15

    ; Debug: Print 'Y' marker after register restore
    DEBUG_SERIAL_CHAR 'Y'

    ; =========================================================================
    ; Step 7: Return to new process
    ; =========================================================================
    ; The return address on the new stack is either:
    ;   - For an existing process: where it called context_switch from
    ;   - For a new process: process_entry_wrapper
    ret


; =============================================================================
; context_switch_first(new_rsp, new_rsp0, new_cr3)
; =============================================================================
; Special context switch for starting the very first process.
;
; Unlike context_switch, this does NOT save the current context because
; there is no valid context to save (we're coming from kernel_main's
; initialization code, not from a process).
;
; Parameters:
;   RDI = new_rsp      - RSP value for first process
;   RSI = new_rsp0     - RSP0 value for TSS
;   RDX = new_cr3      - CR3 value for first process (0 = don't switch)
;
; This function never returns to the caller. It "returns" to the first process.

global context_switch_first
context_switch_first:
    ; =========================================================================
    ; Step 1: Update TSS.RSP0 and syscall kernel stack
    ; =========================================================================
    ; Save RDI (new_rsp), RSI (new_rsp0) and RDX (new_cr3) first
    push rdi
    push rsi
    push rdx

    mov rdi, rsi                ; new_rsp0 as first argument
    call gdt_set_rsp0

    pop rdx
    pop rdi                     ; This is actually rsi (new_rsp0)
    push rdx                    ; Re-save rdx
    call syscall_set_kernel_stack

    pop rdx                     ; Restore new_cr3
    pop rdi                     ; Restore new_rsp

    ; =========================================================================
    ; Step 2: Switch CR3 if needed
    ; =========================================================================
    test rdx, rdx
    jz .skip_cr3_first

    ; Switch to new address space
    mov cr3, rdx

.skip_cr3_first:
    ; =========================================================================
    ; Step 3: Switch to first process's stack
    ; =========================================================================
    mov rsp, rdi

    ; =========================================================================
    ; Step 4: Restore callee-saved registers
    ; =========================================================================
    ; The first process's stack was set up with these pushed
    pop rbx
    pop rbp
    pop r12
    pop r13
    pop r14
    pop r15

    ; =========================================================================
    ; Step 5: "Return" to first process
    ; =========================================================================
    ; The return address is process_entry_wrapper
    ret
