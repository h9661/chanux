; =============================================================================
; Chanux OS - Stage 2 Bootloader
; =============================================================================
; This is the extended bootloader loaded by Stage 1
;
; Responsibilities:
;   1. Enable A20 line (access memory above 1MB)
;   2. Get memory map from BIOS (E820)
;   3. Load kernel from disk to temporary address
;   4. Set up GDT for protected mode
;   5. Enter 32-bit protected mode
;   6. Copy kernel to 1MB
;   7. Set up page tables for long mode
;   8. Enable PAE and long mode
;   9. Jump to kernel
;
; Entry: Real Mode (16-bit), loaded at 0x7E00
;        DL = boot drive number
; =============================================================================

[BITS 16]
[ORG 0x7E00]

%include "boot.inc"

; Temporary kernel load address (in low memory, accessible in real mode)
KERNEL_TEMP_ADDR    equ 0x10000     ; 64KB mark

; =============================================================================
; Stage 2 Entry Point
; =============================================================================
stage2_start:
    ; Set up segment registers (ensure they're 0 for flat addressing)
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; Set up stack below Stage 1

    ; Save boot drive
    mov [boot_drive], dl

    ; Print Stage 2 startup message
    mov si, msg_stage2
    call print_string_16

    ; ==========================================================================
    ; Step 1: Enable A20 Line
    ; ==========================================================================
    mov si, msg_a20
    call print_string_16

    call enable_a20

    mov si, msg_done
    call print_string_16

    ; ==========================================================================
    ; Step 2: Get Memory Map (E820)
    ; ==========================================================================
    mov si, msg_memmap
    call print_string_16

    call get_memory_map

    ; Print OK directly
    mov ah, 0x0E
    mov al, 'O'
    int 0x10
    mov al, 'K'
    int 0x10
    mov al, 0x0D
    int 0x10
    mov al, 0x0A
    int 0x10

    ; ==========================================================================
    ; Step 3: Load Kernel from Disk to Temporary Address
    ; ==========================================================================
    ; Print message inline
    mov ah, 0x0E
    mov al, ' '
    int 0x10
    int 0x10
    mov al, 'L'
    int 0x10
    mov al, 'o'
    int 0x10
    mov al, 'a'
    int 0x10
    mov al, 'd'
    int 0x10
    mov al, 'i'
    int 0x10
    mov al, 'n'
    int 0x10
    mov al, 'g'
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 'k'
    int 0x10
    mov al, 'e'
    int 0x10
    mov al, 'r'
    int 0x10
    mov al, 'n'
    int 0x10
    mov al, 'e'
    int 0x10
    mov al, 'l'
    int 0x10
    mov al, '.'
    int 0x10
    int 0x10
    int 0x10
    mov al, ' '
    int 0x10

    call load_kernel

    ; Print OK inline
    mov ah, 0x0E
    mov al, 'O'
    int 0x10
    mov al, 'K'
    int 0x10
    mov al, 0x0D
    int 0x10
    mov al, 0x0A
    int 0x10

    ; ==========================================================================
    ; Step 4: Enter Protected Mode
    ; ==========================================================================
    mov si, msg_pmode
    call print_string_16

    ; Disable interrupts
    cli

    ; Load 32-bit GDT
    lgdt [gdt32_descriptor]

    ; Enable protected mode (set PE bit in CR0)
    mov eax, cr0
    or eax, CR0_PE
    mov cr0, eax

    ; Far jump to flush pipeline and load CS
    jmp GDT32_CODE:protected_mode_entry

; =============================================================================
; 32-bit Protected Mode Code
; =============================================================================
[BITS 32]
protected_mode_entry:
    ; Set up segment registers with data segment
    mov ax, GDT32_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up stack
    mov esp, KERNEL_STACK_ADDR

    ; Print confirmation (directly to VGA memory)
    mov edi, VGA_MEMORY
    mov esi, msg_pmode_ok
    call print_string_32

    ; ==========================================================================
    ; Step 5: Copy Kernel from Temporary Address to 1MB
    ; ==========================================================================
    ; Now that we're in protected mode, we can access all 4GB of memory

    add edi, 160                ; Next line
    mov esi, msg_copying
    call print_string_32

    ; Copy kernel from KERNEL_TEMP_ADDR to KERNEL_LOAD_ADDR
    mov esi, KERNEL_TEMP_ADDR   ; Source: 0x10000
    mov edi, KERNEL_LOAD_ADDR   ; Destination: 0x100000 (1MB)
    mov ecx, (KERNEL_SECTORS * 512) / 4  ; Number of dwords to copy
    rep movsd

    add edi, 160                ; Next line (edi was modified, recalculate)
    mov edi, VGA_MEMORY + 320
    mov esi, msg_copy_ok
    call print_string_32

    ; ==========================================================================
    ; Step 6: Set Up Page Tables for Long Mode
    ; ==========================================================================
    call setup_page_tables

    ; Print page tables OK
    mov edi, VGA_MEMORY + 480   ; Line 3
    mov esi, msg_pages_ok
    call print_string_32

    ; ==========================================================================
    ; Step 7: Enable PAE and Long Mode
    ; ==========================================================================

    ; Enable PAE (Physical Address Extension)
    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax

    ; Load PML4 address into CR3
    mov eax, PML4_ADDR
    mov cr3, eax

    ; Enable Long Mode in EFER MSR
    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_LME
    wrmsr

    ; Enable paging (this activates long mode)
    mov eax, cr0
    or eax, CR0_PG
    mov cr0, eax

    ; Load 64-bit GDT
    lgdt [gdt64_descriptor]

    ; Jump to 64-bit code segment
    jmp GDT64_CODE:long_mode_entry

; =============================================================================
; 64-bit Long Mode Code
; =============================================================================
[BITS 64]
long_mode_entry:
    ; Set up segment registers
    mov ax, GDT64_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up 64-bit stack
    mov rsp, KERNEL_STACK_ADDR

    ; Print long mode OK (VGA memory)
    mov rdi, VGA_MEMORY + 640   ; Line 4
    mov rsi, msg_lmode_ok
    call print_string_64

    ; ==========================================================================
    ; Step 8: Jump to Kernel
    ; ==========================================================================

    ; Pass boot information to kernel
    ; RDI = pointer to boot info structure (memory map address)
    mov rdi, MEMORY_MAP_ADDR

    ; Jump to kernel entry point at 1MB
    mov rax, KERNEL_LOAD_ADDR
    jmp rax

    ; Should never reach here
.hang:
    cli
    hlt
    jmp .hang

; =============================================================================
; 16-bit Real Mode Functions
; =============================================================================
[BITS 16]

; -----------------------------------------------------------------------------
; enable_a20 - Enable A20 line using multiple methods
; -----------------------------------------------------------------------------
enable_a20:
    pusha

    ; Method 1: BIOS INT 15h, AX=2401h
    mov ax, 0x2401
    int 0x15
    jnc .done                   ; Success if carry clear

    ; Method 2: Keyboard Controller
    call .wait_input
    mov al, 0xAD                ; Disable keyboard
    out 0x64, al

    call .wait_input
    mov al, 0xD0                ; Read from output port
    out 0x64, al

    call .wait_output
    in al, 0x60
    push ax

    call .wait_input
    mov al, 0xD1                ; Write to output port
    out 0x64, al

    call .wait_input
    pop ax
    or al, 2                    ; Set A20 bit
    out 0x60, al

    call .wait_input
    mov al, 0xAE                ; Enable keyboard
    out 0x64, al

    call .wait_input
    jmp .done

.wait_input:
    in al, 0x64
    test al, 2
    jnz .wait_input
    ret

.wait_output:
    in al, 0x64
    test al, 1
    jz .wait_output
    ret

.done:
    popa
    ret

; -----------------------------------------------------------------------------
; get_memory_map - Get memory map using BIOS E820
; -----------------------------------------------------------------------------
get_memory_map:
    pusha                       ; Save all 16-bit registers
    push es

    ; Ensure ES is 0
    xor ax, ax
    mov es, ax

    mov di, MEMORY_MAP_ADDR + 4 ; Start storing entries after count
    xor ebx, ebx                ; Continuation value (0 for first call)
    xor bp, bp                  ; Entry counter

.loop:
    mov eax, 0xE820             ; E820 function
    mov ecx, 24                 ; Size of entry (24 bytes)
    mov edx, 0x534D4150         ; "SMAP" signature
    int 0x15

    jc .done                    ; Carry set = error or end
    cmp eax, 0x534D4150         ; Verify signature
    jne .done

    inc bp                      ; Increment counter
    add di, 24                  ; Move to next entry

    test ebx, ebx               ; If EBX is 0, we're done
    jz .done
    jmp .loop

.done:
    ; Store entry count at MEMORY_MAP_ADDR
    mov ax, bp
    mov [MEMORY_MAP_ADDR], ax

    pop es
    popa                        ; Restore all 16-bit registers
    ret

; -----------------------------------------------------------------------------
; load_kernel - Load kernel from disk to temporary address
; -----------------------------------------------------------------------------
; Uses BIOS INT 13h with chunked reads for compatibility
; -----------------------------------------------------------------------------
load_kernel:
    pusha

    ; Initialize variables
    mov word [sectors_remaining], KERNEL_SECTORS
    mov dword [current_lba], KERNEL_START_SECTOR
    mov word [current_segment], KERNEL_TEMP_ADDR >> 4  ; 0x1000

.read_loop:
    ; Determine how many sectors to read (max 64 per call for safety)
    mov ax, [sectors_remaining]
    test ax, ax
    jz .done                    ; Done if no sectors remaining

    cmp ax, 64
    jbe .use_remaining
    mov ax, 64
.use_remaining:

    ; Fill in DAP
    mov word [dap_count], ax
    mov word [dap_offset], 0
    mov cx, [current_segment]
    mov word [dap_segment], cx
    mov ecx, [current_lba]
    mov dword [dap_lba], ecx
    mov dword [dap_lba + 4], 0

    ; Perform disk read
    mov ah, 0x42                ; Extended read
    mov dl, [boot_drive]
    mov si, dap
    int 0x13

    jc .error

    ; Update counters
    mov ax, [dap_count]
    sub [sectors_remaining], ax

    ; Add to LBA (32-bit add)
    movzx eax, word [dap_count]
    add [current_lba], eax

    ; Update segment (add sectors * 512 / 16 = sectors * 32)
    mov ax, [dap_count]
    shl ax, 5                   ; * 32
    add [current_segment], ax

    jmp .read_loop

.done:
    popa
    ret

.error:
    mov si, msg_disk_error
    call print_string_16
    cli
    hlt

; Variables for load_kernel
sectors_remaining: dw 0
current_lba:       dd 0
current_segment:   dw 0

; Disk Address Packet for extended read
align 4
dap:
    db 0x10                     ; Size of DAP (16 bytes)
    db 0                        ; Reserved
dap_count:
    dw 0                        ; Number of sectors to read
dap_offset:
    dw 0                        ; Offset (always 0)
dap_segment:
    dw 0                        ; Segment
dap_lba:
    dq 0                        ; LBA (64-bit)

; -----------------------------------------------------------------------------
; print_string_16 - Print string in real mode
; Input: SI = pointer to string
; -----------------------------------------------------------------------------
print_string_16:
    pusha
    mov ah, 0x0E

.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop

.done:
    popa
    ret

; =============================================================================
; 32-bit Protected Mode Functions
; =============================================================================
[BITS 32]

; -----------------------------------------------------------------------------
; setup_page_tables - Set up identity mapping and higher-half mapping
; -----------------------------------------------------------------------------
setup_page_tables:
    pusha

    ; Clear page table memory (16KB: PML4, PDPT, PD, PT)
    mov edi, PML4_ADDR
    mov ecx, 4096               ; 16KB / 4 = 4096 dwords
    xor eax, eax
    rep stosd

    ; ==========================================================================
    ; Set up identity mapping for first 2MB (using 2MB huge pages)
    ; This maps virtual 0x0 -> physical 0x0
    ; ==========================================================================

    ; PML4[0] -> PDPT
    mov eax, PDPT_ADDR
    or eax, PAGE_FLAGS
    mov [PML4_ADDR], eax

    ; PDPT[0] -> PD
    mov eax, PD_ADDR
    or eax, PAGE_FLAGS
    mov [PDPT_ADDR], eax

    ; PD[0] -> 2MB huge page at physical 0
    mov eax, 0x00000000
    or eax, PAGE_FLAGS_HUGE
    mov [PD_ADDR], eax

    ; ==========================================================================
    ; Set up higher-half mapping for kernel
    ; Virtual 0xFFFFFFFF80000000 -> Physical 0x00000000
    ; ==========================================================================

    ; PML4[511] -> PDPT (for higher half)
    ; Entry 511 covers addresses starting with 0xFFFF8...
    mov eax, PDPT_ADDR
    or eax, PAGE_FLAGS
    mov [PML4_ADDR + 511*8], eax

    ; PDPT[510] -> PD (for -2GB, i.e., 0xFFFFFFFF80000000)
    mov eax, PD_ADDR
    or eax, PAGE_FLAGS
    mov [PDPT_ADDR + 510*8], eax

    ; PD[0] is already set up for the 2MB page
    ; Both identity and higher-half point to same PD, so both mappings work

    popa
    ret

; -----------------------------------------------------------------------------
; print_string_32 - Print string to VGA memory (32-bit mode)
; Input: EDI = VGA memory address, ESI = string pointer
; -----------------------------------------------------------------------------
print_string_32:
    push eax
    push esi

.loop:
    lodsb
    test al, al
    jz .done
    mov ah, VGA_WHITE_ON_BLACK
    mov [edi], ax
    add edi, 2
    jmp .loop

.done:
    pop esi
    pop eax
    ret

; =============================================================================
; 64-bit Long Mode Functions
; =============================================================================
[BITS 64]

; -----------------------------------------------------------------------------
; print_string_64 - Print string to VGA memory (64-bit mode)
; Input: RDI = VGA memory address, RSI = string pointer
; -----------------------------------------------------------------------------
print_string_64:
    push rax
    push rsi

.loop:
    lodsb
    test al, al
    jz .done
    mov ah, VGA_WHITE_ON_BLACK
    mov [rdi], ax
    add rdi, 2
    jmp .loop

.done:
    pop rsi
    pop rax
    ret

; =============================================================================
; GDT Structures
; =============================================================================

; 32-bit GDT
gdt32_start:
    ; Null descriptor
    dq 0

    ; Code segment (0x08)
    dw 0xFFFF                   ; Limit (low)
    dw 0x0000                   ; Base (low)
    db 0x00                     ; Base (middle)
    db 10011010b                ; Access: Present, Ring 0, Code, Executable, Readable
    db 11001111b                ; Flags: 4KB granularity, 32-bit + Limit (high)
    db 0x00                     ; Base (high)

    ; Data segment (0x10)
    dw 0xFFFF                   ; Limit (low)
    dw 0x0000                   ; Base (low)
    db 0x00                     ; Base (middle)
    db 10010010b                ; Access: Present, Ring 0, Data, Writable
    db 11001111b                ; Flags: 4KB granularity, 32-bit + Limit (high)
    db 0x00                     ; Base (high)
gdt32_end:

gdt32_descriptor:
    dw gdt32_end - gdt32_start - 1  ; Size
    dd gdt32_start                   ; Address

; 64-bit GDT
gdt64_start:
    ; Null descriptor
    dq 0

    ; Code segment (0x08) - 64-bit
    dw 0x0000                   ; Limit (ignored in 64-bit mode)
    dw 0x0000                   ; Base (low)
    db 0x00                     ; Base (middle)
    db 10011010b                ; Access: Present, Ring 0, Code, Executable, Readable
    db 00100000b                ; Flags: Long mode
    db 0x00                     ; Base (high)

    ; Data segment (0x10) - 64-bit
    dw 0x0000                   ; Limit (ignored)
    dw 0x0000                   ; Base (low)
    db 0x00                     ; Base (middle)
    db 10010010b                ; Access: Present, Ring 0, Data, Writable
    db 00000000b                ; Flags
    db 0x00                     ; Base (high)
gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1  ; Size
    dq gdt64_start                   ; Address (64-bit)

; =============================================================================
; Data
; =============================================================================
boot_drive:     db 0

msg_stage2:     db "Chanux Stage 2 Bootloader", 0x0D, 0x0A, 0
msg_a20:        db "  Enabling A20 line... ", 0
msg_memmap:     db "  Getting memory map... ", 0
msg_kernel:     db "  Loading kernel... ", 0
msg_pmode:      db "  Entering protected mode...", 0x0D, 0x0A, 0
msg_done:       db "OK", 0x0D, 0x0A, 0
msg_disk_error: db "Disk error!", 0x0D, 0x0A, 0

msg_pmode_ok:   db "Protected Mode (32-bit) - OK", 0
msg_copying:    db "Copying kernel to 1MB...", 0
msg_copy_ok:    db "Kernel copy complete", 0
msg_pages_ok:   db "Page Tables configured", 0
msg_lmode_ok:   db "Long Mode (64-bit) - Jumping to kernel...", 0

; Pad to align and ensure we have space
times 16384 - ($ - $$) db 0     ; Pad to 16KB
