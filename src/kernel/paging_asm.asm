; Paging Assembly Routines
; This file contains low-level assembly functions for paging operations

[BITS 32]

; Export functions
global enable_paging
global read_cr2
global read_cr3
global write_cr3
global invlpg

; enable_paging - Enable paging by setting CR0.PG bit
; Parameter: physical address of page directory (passed on stack)
enable_paging:
    push ebp
    mov ebp, esp
    
    ; Get page directory address from stack
    mov eax, [ebp + 8]
    
    ; Load page directory address into CR3
    mov cr3, eax
    
    ; Enable paging by setting bit 31 of CR0
    mov eax, cr0
    or eax, 0x80000000    ; Set PG bit (bit 31)
    mov cr0, eax
    
    ; Jump to flush the instruction pipeline
    jmp .flush
.flush:
    
    pop ebp
    ret

; read_cr2 - Read CR2 register (contains page fault address)
; Returns: Value of CR2 in EAX
read_cr2:
    mov eax, cr2
    ret

; read_cr3 - Read CR3 register (contains page directory address)
; Returns: Value of CR3 in EAX
read_cr3:
    mov eax, cr3
    ret

; write_cr3 - Write CR3 register (load new page directory)
; Parameter: New CR3 value (passed on stack)
write_cr3:
    push ebp
    mov ebp, esp
    
    mov eax, [ebp + 8]
    mov cr3, eax
    
    pop ebp
    ret

; invlpg - Invalidate page in TLB
; Parameter: Virtual address to invalidate (passed on stack)
invlpg:
    push ebp
    mov ebp, esp
    
    mov eax, [ebp + 8]
    invlpg [eax]
    
    pop ebp
    ret