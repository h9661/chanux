; System Call Assembly Handler
; This file contains the low-level interrupt handler for system calls

[BITS 32]

; External C handler
extern syscall_handler

; Export the interrupt handler
global isr128

; System call interrupt handler (int 0x80)
; Unlike hardware interrupts, system calls don't push an error code
isr128:
    cli                 ; Disable interrupts
    
    ; Push a dummy error code (0) to maintain stack structure
    push dword 0
    
    ; Push interrupt number (128 = 0x80)
    push dword 128
    
    ; Save all general purpose registers
    pusha
    
    ; Save data segment
    mov ax, ds
    push eax
    
    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Push pointer to register structure as parameter
    push esp
    
    ; Call C system call handler
    call syscall_handler
    
    ; Clean up parameter
    add esp, 4
    
    ; The return value is in EAX, save it
    mov [esp + 32], eax  ; Overwrite saved EAX with return value
    
    ; Restore data segment
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Restore registers (EAX will have the return value)
    popa
    
    ; Remove interrupt number and error code
    add esp, 8
    
    ; Return from interrupt
    sti             ; Re-enable interrupts
    iret