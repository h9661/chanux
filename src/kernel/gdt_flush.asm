; GDT Flush Assembly Routine
; This code loads the Global Descriptor Table and updates segment registers

[BITS 32]  ; Generate 32-bit code

global gdt_flush  ; Make gdt_flush visible to C code
extern gp         ; GDT pointer structure defined in gdt.c

; gdt_flush - Load GDT and update segment registers
; Parameter: pointer to GDT pointer structure (passed on stack)
gdt_flush:
    ; Get the parameter from stack (GDT pointer address)
    mov eax, [esp+4]
    
    ; Load the GDT
    ; LGDT instruction loads the GDTR register with the base and limit
    ; of the Global Descriptor Table
    lgdt [eax]
    
    ; Update all data segment registers to point to the new data segment
    ; 0x10 is the offset of the data segment in the GDT (third entry = 2 * 8 bytes)
    mov ax, 0x10
    mov ds, ax    ; Data segment
    mov es, ax    ; Extra segment
    mov fs, ax    ; FS segment
    mov gs, ax    ; GS segment
    mov ss, ax    ; Stack segment
    
    ; Perform a far jump to update the code segment register (CS)
    ; 0x08 is the offset of the code segment in the GDT (second entry = 1 * 8 bytes)
    ; This jump flushes the CPU instruction pipeline
    jmp 0x08:.flush
    
.flush:
    ; Return to caller
    ; All segment registers now point to the new GDT entries
    ret