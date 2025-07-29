; IDT Load Assembly Routine
; This code loads the Interrupt Descriptor Table

[BITS 32]  ; Generate 32-bit code

global idt_load  ; Make idt_load visible to C code

; idt_load - Load IDT into IDTR register
; Parameter: pointer to IDT pointer structure (passed on stack)
; 
; The IDTR (Interrupt Descriptor Table Register) is a special register
; that holds the base address and limit of the IDT. When an interrupt
; occurs, the CPU uses this register to locate the IDT.
idt_load:
    ; Get the parameter from stack (IDT pointer address)
    ; In x86 calling convention, parameters are pushed on stack
    ; [esp] contains return address, [esp+4] contains first parameter
    mov eax, [esp+4]
    
    ; Load the IDT
    ; LIDT instruction loads the IDTR register with the base and limit
    ; of the Interrupt Descriptor Table from the memory location in EAX
    lidt [eax]
    
    ; Return to caller
    ; The IDT is now active and the CPU will use it for interrupt handling
    ret