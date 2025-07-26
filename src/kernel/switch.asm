; Context Switch Assembly Code
; Performs the low-level context switch between processes

[BITS 32]
section .text

global switch_context
global process_start

; switch_context(cpu_context_t *old, cpu_context_t *new)
; Save current context to 'old' and load context from 'new'
switch_context:
    ; Get parameters
    mov eax, [esp + 4]    ; old context pointer
    mov edx, [esp + 8]    ; new context pointer
    
    ; Save current context
    push ebp
    push ebx
    push esi
    push edi
    
    ; Save current stack pointer to old context
    mov [eax + 12], esp   ; offset 12 = esp field
    
    ; Load new stack pointer
    mov esp, [edx + 12]   ; offset 12 = esp field
    
    ; Restore new context
    pop edi
    pop esi
    pop ebx
    pop ebp
    
    ret

; process_start()
; Entry point for new processes - pops initial context from stack
process_start:
    ; The stack has been set up with initial values
    ; Pop them to start the process
    pop eax
    pop ecx
    pop edx
    pop ebx
    pop ebp
    pop esi
    pop edi
    
    ; Enable interrupts
    sti
    
    ; Return will pop EIP and start execution
    ret