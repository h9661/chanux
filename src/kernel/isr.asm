; Interrupt Service Routines Assembly
; This file contains ISR stubs and the page fault handler

[BITS 32]

; External C functions
extern vmm_page_fault_handler

; Export ISR
global isr14

; Common ISR stub that saves context
isr_common_stub:
    ; Save all registers
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
    
    ; Call C handler (error code and interrupt number already on stack)
    call vmm_page_fault_handler
    add esp, 8  ; Clean up pushed error code and interrupt number
    
    ; Restore data segment
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Restore registers
    popa
    
    ; Clean up error code and interrupt number
    add esp, 8
    
    ; Return from interrupt
    iret

; ISR 14: Page Fault
; The CPU automatically pushes:
; - Error code
; - EIP
; - CS
; - EFLAGS
; - ESP (if privilege level change)
; - SS (if privilege level change)
isr14:
    cli                 ; Disable interrupts
    
    ; The error code is already pushed by CPU
    ; Push a dummy interrupt number (14)
    push dword 14
    
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
    
    ; Get CR2 (faulting address) and error code for handler
    mov eax, cr2
    push eax        ; Push faulting address
    
    ; Error code is at esp + 48 (after pusha and ds)
    mov eax, [esp + 48]
    push eax        ; Push error code
    
    ; Call C page fault handler
    call vmm_page_fault_handler
    add esp, 8      ; Clean up parameters
    
    ; Restore data segment
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Restore registers
    popa
    
    ; Remove interrupt number and error code
    add esp, 8
    
    ; Return from interrupt
    sti             ; Re-enable interrupts
    iret