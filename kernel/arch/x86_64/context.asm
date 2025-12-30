; =============================================================================
; Chanux OS - Context Switch Assembly Routines
; =============================================================================
; Implements low-level context switching between kernel processes.
;
; Context switch strategy:
;   - Save callee-saved registers (System V AMD64 ABI)
;   - Save current RSP to old process's PCB
;   - Update TSS.RSP0 for new process
;   - Load new RSP
;   - Restore callee-saved registers from new stack
;   - "Return" to new process
;
; The stack setup for each process ensures that when context_switch returns,
; it returns to the process's saved instruction pointer.
; =============================================================================

[BITS 64]

section .text

; External C function for updating TSS.RSP0
extern gdt_set_rsp0

; =============================================================================
; context_switch(old_rsp_ptr, new_rsp, new_rsp0)
; =============================================================================
; Performs a context switch between two kernel processes.
;
; Parameters (System V AMD64 ABI):
;   RDI = old_rsp_ptr  - Pointer to save current RSP (uint64_t* in old PCB)
;   RSI = new_rsp      - RSP value to load for new process
;   RDX = new_rsp0     - RSP0 value for TSS (kernel stack top of new process)
;
; This function:
;   1. Saves callee-saved registers (rbx, rbp, r12-r15) on current stack
;   2. Saves current RSP to *old_rsp_ptr (old process's PCB.rsp)
;   3. Updates TSS.RSP0 with new_rsp0
;   4. Loads new RSP (switches to new process's stack)
;   5. Restores callee-saved registers from new stack
;   6. Returns (to whatever address is at top of new stack - the new process)
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
    ; Step 3: Update TSS.RSP0 for new process
    ; =========================================================================
    ; Need to call gdt_set_rsp0(new_rsp0)
    ; RDX contains new_rsp0, but gdt_set_rsp0 expects argument in RDI
    ; Save RSI (new_rsp) first as gdt_set_rsp0 may clobber it
    push rsi
    push rdx                    ; Also save in case we need it
    mov rdi, rdx                ; new_rsp0 as first argument
    call gdt_set_rsp0
    pop rdx
    pop rsi

    ; =========================================================================
    ; Step 4: Switch to new process's stack
    ; =========================================================================
    mov rsp, rsi

    ; =========================================================================
    ; Step 5: Restore callee-saved registers from new stack
    ; =========================================================================
    ; Order must match what we pushed in step 1!
    pop rbx
    pop rbp
    pop r12
    pop r13
    pop r14
    pop r15

    ; =========================================================================
    ; Step 6: Return to new process
    ; =========================================================================
    ; The return address on the new stack is either:
    ;   - For an existing process: where it called context_switch from
    ;   - For a new process: process_entry_wrapper
    ret


; =============================================================================
; context_switch_first(new_rsp, new_rsp0)
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
;
; This function never returns to the caller. It "returns" to the first process.

global context_switch_first
context_switch_first:
    ; =========================================================================
    ; Step 1: Update TSS.RSP0
    ; =========================================================================
    ; Save RDI (new_rsp) first
    push rdi
    mov rdi, rsi                ; new_rsp0 as first argument
    call gdt_set_rsp0
    pop rdi

    ; =========================================================================
    ; Step 2: Switch to first process's stack
    ; =========================================================================
    mov rsp, rdi

    ; =========================================================================
    ; Step 3: Restore callee-saved registers
    ; =========================================================================
    ; The first process's stack was set up with these pushed
    pop rbx
    pop rbp
    pop r12
    pop r13
    pop r14
    pop r15

    ; =========================================================================
    ; Step 4: "Return" to first process
    ; =========================================================================
    ; The return address is process_entry_wrapper
    ret
