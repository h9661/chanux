; =============================================================================
; Chanux OS - User Mode Entry (x86_64)
; =============================================================================
; This file implements the transition from kernel mode to user mode using
; the IRETQ instruction.
;
; IRETQ Stack Frame (pushed in order, popped in reverse):
;   +40: SS      (User stack segment)
;   +32: RSP     (User stack pointer)
;   +24: RFLAGS  (User flags, with IF=1)
;   +16: CS      (User code segment)
;   +8:  RIP     (User entry point)
;   +0:  Error code (not used, we set this up manually)
;
; The IRETQ instruction atomically:
;   - Pops RIP (jumps to user code)
;   - Pops CS  (sets code segment to Ring 3)
;   - Pops RFLAGS (restores flags)
;   - Pops RSP (sets user stack)
;   - Pops SS  (sets stack segment to Ring 3)
; =============================================================================

section .text
    bits 64

; =============================================================================
; User Mode Entry Segments
; =============================================================================
; These must match the GDT setup in gdt.c:
;   User Data = 0x28 | RPL 3 = 0x2B
;   User Code = 0x30 | RPL 3 = 0x33

USER_DATA_SEGMENT equ 0x2B      ; GDT_USER_DATA_RPL
USER_CODE_SEGMENT equ 0x33      ; GDT_USER_CODE_RPL

; RFLAGS bits
RFLAGS_IF         equ (1 << 9)  ; Interrupt Flag
RFLAGS_RESERVED   equ (1 << 1)  ; Reserved, always 1

; =============================================================================
; user_mode_enter - Enter User Mode via IRETQ
; =============================================================================
; void user_mode_enter(uint64_t entry_point, uint64_t user_stack)
;
; Arguments:
;   RDI = entry_point (user RIP)
;   RSI = user_stack  (user RSP)
;
; This function does not return.
; =============================================================================

global user_mode_enter
user_mode_enter:
    ; Disable interrupts while we set up the stack frame
    cli

    ; -------------------------------------------------------------------------
    ; Set data segment registers to user data segment
    ; -------------------------------------------------------------------------
    mov ax, USER_DATA_SEGMENT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ; Note: SS will be set by IRETQ

    ; -------------------------------------------------------------------------
    ; Build IRETQ stack frame
    ; -------------------------------------------------------------------------
    ; We need to push in reverse order of how IRETQ pops

    ; Push SS (user stack segment)
    push USER_DATA_SEGMENT

    ; Push RSP (user stack pointer)
    push rsi

    ; Push RFLAGS (enable interrupts in user mode)
    ; Set IF=1 so interrupts work in user mode
    pushfq
    pop rax
    or rax, RFLAGS_IF           ; Enable interrupts
    push rax

    ; Push CS (user code segment with RPL 3)
    push USER_CODE_SEGMENT

    ; Push RIP (user entry point)
    push rdi

    ; -------------------------------------------------------------------------
    ; Clear general purpose registers for security
    ; -------------------------------------------------------------------------
    ; Don't leak kernel data to user mode
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    xor rbp, rbp

    ; -------------------------------------------------------------------------
    ; Enter user mode!
    ; -------------------------------------------------------------------------
    ; IRETQ will pop: RIP, CS, RFLAGS, RSP, SS
    iretq

; =============================================================================
; user_mode_enter_with_args - Enter User Mode with arguments
; =============================================================================
; void user_mode_enter_with_args(uint64_t entry, uint64_t stack,
;                                uint64_t arg1, uint64_t arg2)
;
; Arguments:
;   RDI = entry_point
;   RSI = user_stack
;   RDX = arg1 (passed to user in RDI)
;   RCX = arg2 (passed to user in RSI)
;
; This function does not return.
; =============================================================================

global user_mode_enter_with_args
user_mode_enter_with_args:
    ; Save arguments for user mode
    mov r8, rdx                 ; arg1 -> r8 temporarily
    mov r9, rcx                 ; arg2 -> r9 temporarily
    mov r10, rdi                ; entry -> r10
    mov r11, rsi                ; stack -> r11

    ; Disable interrupts
    cli

    ; Set data segment registers
    mov ax, USER_DATA_SEGMENT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Build IRETQ stack frame
    push USER_DATA_SEGMENT      ; SS
    push r11                    ; RSP (user stack)

    pushfq
    pop rax
    or rax, RFLAGS_IF
    push rax                    ; RFLAGS

    push USER_CODE_SEGMENT      ; CS
    push r10                    ; RIP (entry point)

    ; Set up user arguments
    mov rdi, r8                 ; arg1
    mov rsi, r9                 ; arg2

    ; Clear other registers
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    xor rbp, rbp

    ; Enter user mode
    iretq
