; IRQ Handler Assembly Stubs
; 
; This file contains the low-level IRQ handler stubs that save CPU state,
; call the C handler, and restore state before returning.

section .text

; External C function
extern irq_handler

; Macro to create IRQ handler stub
%macro IRQ_HANDLER 1
global irq%1
irq%1:
    cli                     ; Disable interrupts
    push dword 0            ; Push dummy error code
    push dword %1           ; Push IRQ number
    jmp irq_common_stub
%endmacro

; Create handlers for all 16 IRQs
IRQ_HANDLER 0   ; Programmable Interval Timer
IRQ_HANDLER 1   ; Keyboard
IRQ_HANDLER 2   ; Cascade
IRQ_HANDLER 3   ; COM2
IRQ_HANDLER 4   ; COM1
IRQ_HANDLER 5   ; LPT2
IRQ_HANDLER 6   ; Floppy
IRQ_HANDLER 7   ; LPT1
IRQ_HANDLER 8   ; Real-time clock
IRQ_HANDLER 9   ; Available
IRQ_HANDLER 10  ; Available
IRQ_HANDLER 11  ; Available
IRQ_HANDLER 12  ; PS/2 Mouse
IRQ_HANDLER 13  ; FPU/Coprocessor
IRQ_HANDLER 14  ; Primary IDE
IRQ_HANDLER 15  ; Secondary IDE

; Common IRQ handler code
irq_common_stub:
    ; Save CPU state
    pusha               ; Push all general purpose registers
    
    ; Save segment registers
    push ds
    push es
    push fs
    push gs
    
    ; Load kernel data segment
    mov ax, 0x10        ; Kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Push stack pointer for handler
    push esp
    
    ; Call C handler
    ; Stack: [gs][fs][es][ds][edi][esi][ebp][esp][ebx][edx][ecx][eax][irq_num][err_code][eip][cs][eflags]
    mov eax, [esp + 48] ; Get IRQ number (after all pushes)
    push eax            ; Push IRQ number as argument
    call irq_handler
    add esp, 8          ; Clean up arguments (IRQ number + ESP)
    
    ; Restore segment registers
    pop gs
    pop fs
    pop es
    pop ds
    
    ; Restore general purpose registers
    popa
    
    ; Clean up IRQ number and error code
    add esp, 8
    
    ; Enable interrupts and return
    sti
    iret