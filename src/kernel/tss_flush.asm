; Task State Segment Loading
; Load the TSS selector into the task register

[BITS 32]
section .text

global tss_flush

tss_flush:
    mov ax, 0x2B    ; TSS selector (GDT entry 5: 0x28 | RPL 3 = 0x2B)
    ltr ax          ; Load task register with TSS selector
    ret