; =============================================================================
; Chanux OS - IDT Assembly Routines
; =============================================================================
; Contains:
;   - ISR stubs for exceptions (vectors 0-31)
;   - IRQ stubs for hardware interrupts (vectors 32-47)
;   - Common handler that saves registers and calls C handlers
;   - IDT loading routine
;
; The ISR stubs push the interrupt number and (if needed) a dummy error code,
; then jump to a common routine that saves all registers and calls the C handler.
; =============================================================================

[BITS 64]

section .text

; =============================================================================
; External C Handlers
; =============================================================================

extern isr_handler          ; C handler for exceptions
extern irq_handler          ; C handler for IRQs

; =============================================================================
; ISR Stub Macros
; =============================================================================

; Macro for ISR without error code
; CPU doesn't push error code, so we push a dummy 0
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0            ; Push dummy error code
    push qword %1           ; Push interrupt number
    jmp isr_common_stub
%endmacro

; Macro for ISR with error code
; CPU already pushed error code, just push interrupt number
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    ; Error code already pushed by CPU
    push qword %1           ; Push interrupt number
    jmp isr_common_stub
%endmacro

; Macro for IRQ handlers
%macro IRQ 2
global irq%1
irq%1:
    push qword 0            ; Push dummy error code
    push qword %2           ; Push interrupt vector number
    jmp irq_common_stub
%endmacro

; =============================================================================
; Exception ISRs (Vectors 0-31)
; =============================================================================
; Some exceptions push an error code, others don't.
; See Intel SDM Vol. 3A, Table 6-1

ISR_NOERRCODE 0             ; #DE - Divide Error
ISR_NOERRCODE 1             ; #DB - Debug
ISR_NOERRCODE 2             ; NMI - Non-Maskable Interrupt
ISR_NOERRCODE 3             ; #BP - Breakpoint
ISR_NOERRCODE 4             ; #OF - Overflow
ISR_NOERRCODE 5             ; #BR - Bound Range Exceeded
ISR_NOERRCODE 6             ; #UD - Invalid Opcode
ISR_NOERRCODE 7             ; #NM - Device Not Available
ISR_ERRCODE   8             ; #DF - Double Fault (error code = 0)
ISR_NOERRCODE 9             ; Coprocessor Segment Overrun (reserved)
ISR_ERRCODE   10            ; #TS - Invalid TSS
ISR_ERRCODE   11            ; #NP - Segment Not Present
ISR_ERRCODE   12            ; #SS - Stack Segment Fault
ISR_ERRCODE   13            ; #GP - General Protection Fault
ISR_ERRCODE   14            ; #PF - Page Fault
ISR_NOERRCODE 15            ; Reserved
ISR_NOERRCODE 16            ; #MF - x87 FPU Floating-Point Error
ISR_ERRCODE   17            ; #AC - Alignment Check
ISR_NOERRCODE 18            ; #MC - Machine Check
ISR_NOERRCODE 19            ; #XM - SIMD Floating-Point Exception
ISR_NOERRCODE 20            ; #VE - Virtualization Exception
ISR_ERRCODE   21            ; #CP - Control Protection Exception
ISR_NOERRCODE 22            ; Reserved
ISR_NOERRCODE 23            ; Reserved
ISR_NOERRCODE 24            ; Reserved
ISR_NOERRCODE 25            ; Reserved
ISR_NOERRCODE 26            ; Reserved
ISR_NOERRCODE 27            ; Reserved
ISR_NOERRCODE 28            ; Reserved
ISR_NOERRCODE 29            ; Reserved
ISR_NOERRCODE 30            ; Reserved
ISR_NOERRCODE 31            ; Reserved

; =============================================================================
; IRQ Handlers (Vectors 32-47)
; =============================================================================
; Hardware IRQs 0-15 are remapped to vectors 32-47

IRQ 0,  32                  ; IRQ0 - PIT Timer
IRQ 1,  33                  ; IRQ1 - Keyboard
IRQ 2,  34                  ; IRQ2 - Cascade (slave PIC)
IRQ 3,  35                  ; IRQ3 - COM2
IRQ 4,  36                  ; IRQ4 - COM1
IRQ 5,  37                  ; IRQ5 - LPT2
IRQ 6,  38                  ; IRQ6 - Floppy Disk
IRQ 7,  39                  ; IRQ7 - LPT1 / Spurious
IRQ 8,  40                  ; IRQ8 - CMOS RTC
IRQ 9,  41                  ; IRQ9 - ACPI
IRQ 10, 42                  ; IRQ10 - Open
IRQ 11, 43                  ; IRQ11 - Open
IRQ 12, 44                  ; IRQ12 - PS/2 Mouse
IRQ 13, 45                  ; IRQ13 - FPU
IRQ 14, 46                  ; IRQ14 - Primary ATA
IRQ 15, 47                  ; IRQ15 - Secondary ATA

; =============================================================================
; Common ISR Stub
; =============================================================================
; Saves all general-purpose registers, calls C handler, restores registers.
;
; Stack layout when entering:
;   [rsp+40] SS
;   [rsp+32] RSP
;   [rsp+24] RFLAGS
;   [rsp+16] CS
;   [rsp+8]  RIP
;   [rsp+0]  Error Code (or dummy 0)
;   [rsp-8]  Interrupt Number (we pushed this)
;
; After saving GPRs, stack matches registers_t structure.

isr_common_stub:
    ; Save all general-purpose registers
    ; Push in reverse order so registers_t struct is correct
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to registers_t as first argument (System V ABI)
    mov rdi, rsp

    ; Call C handler
    ; The handler may modify registers (for task switching later)
    call isr_handler

    ; Restore all general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove interrupt number and error code from stack
    add rsp, 16

    ; Return from interrupt
    ; This pops RIP, CS, RFLAGS, RSP, SS
    iretq

; =============================================================================
; Common IRQ Stub
; =============================================================================
; Same as ISR stub but calls irq_handler instead.

irq_common_stub:
    ; Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to registers_t as first argument
    mov rdi, rsp

    ; Call C handler
    call irq_handler

    ; Restore all general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove interrupt number and error code from stack
    add rsp, 16

    ; Return from interrupt
    iretq

; =============================================================================
; Load IDT
; =============================================================================
; void idt_load(idt_ptr_t* idtr)
; Loads the IDT register with the given IDT pointer.

global idt_load
idt_load:
    lidt [rdi]              ; Load IDT from pointer in RDI (System V ABI)
    ret

; =============================================================================
; ISR Stub Table
; =============================================================================
; Table of ISR stub addresses for use by idt_init()

section .data

global isr_stub_table
isr_stub_table:
    ; Exception stubs (0-31)
    dq isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
    dq isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
    dq isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    dq isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    ; IRQ stubs (32-47)
    dq irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7
    dq irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15
