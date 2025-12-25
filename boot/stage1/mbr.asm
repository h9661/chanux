; =============================================================================
; Chanux OS - Stage 1 Bootloader (MBR)
; =============================================================================
; This is the Master Boot Record - the first 512 bytes loaded by BIOS at 0x7C00
;
; Responsibilities:
;   1. Set up segment registers and stack
;   2. Load Stage 2 bootloader from disk
;   3. Jump to Stage 2
;
; Memory Map at this stage:
;   0x0000 - 0x04FF : Interrupt Vector Table / BIOS Data Area
;   0x0500 - 0x7BFF : Free (we use 0x7C00 - 0x200 for stack)
;   0x7C00 - 0x7DFF : This bootloader (512 bytes)
;   0x7E00 - onwards: Stage 2 will be loaded here
; =============================================================================

[BITS 16]                       ; We start in 16-bit Real Mode
[ORG 0x7C00]                    ; BIOS loads us at this address

; =============================================================================
; Constants
; =============================================================================
STAGE2_LOAD_ADDR    equ 0x7E00  ; Where to load Stage 2
STAGE2_SECTORS      equ 33      ; Number of sectors to load (16KB + buffer)
STAGE2_START_SECTOR equ 2       ; Stage 2 starts at sector 2 (sector 1 is MBR, 1-indexed CHS)

; =============================================================================
; Entry Point
; =============================================================================
start:
    ; Clear interrupts during setup
    cli

    ; Set up segment registers
    ; BIOS might load us at 0x0000:0x7C00 or 0x07C0:0x0000
    ; We normalize to 0x0000:0x7C00
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Set up stack just below bootloader
    mov sp, 0x7C00

    ; Re-enable interrupts
    sti

    ; Save boot drive number (BIOS passes it in DL)
    mov [boot_drive], dl

    ; Print welcome message
    mov si, msg_loading
    call print_string

    ; Reset disk system
    call reset_disk

    ; Load Stage 2 from disk
    call load_stage2

    ; Print success message
    mov si, msg_loaded
    call print_string

    ; Jump to Stage 2
    ; Pass boot drive in DL
    mov dl, [boot_drive]
    jmp STAGE2_LOAD_ADDR

; =============================================================================
; reset_disk - Reset the disk system
; =============================================================================
reset_disk:
    pusha
    xor ax, ax                  ; AH = 0 (reset disk)
    mov dl, [boot_drive]        ; Drive number
    int 0x13                    ; BIOS disk interrupt
    jc .error                   ; Jump if carry flag set (error)
    popa
    ret

.error:
    mov si, msg_disk_error
    call print_string
    jmp hang

; =============================================================================
; load_stage2 - Load Stage 2 bootloader from disk using LBA (INT 13h AH=42h)
; =============================================================================
; Uses extended read for better compatibility and larger reads
; =============================================================================
load_stage2:
    pusha

    ; Check if extended disk functions are available
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .use_chs                 ; Fall back to CHS if not available
    cmp bx, 0xAA55
    jne .use_chs

    ; Use extended read (LBA)
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc .error
    popa
    ret

.use_chs:
    ; Fall back to CHS read - load in smaller chunks
    xor ax, ax
    mov es, ax
    mov bx, STAGE2_LOAD_ADDR    ; ES:BX = 0x0000:0x7E00
    mov cx, STAGE2_SECTORS
    mov [.remaining], cx
    mov byte [.sector], STAGE2_START_SECTOR

.read_loop:
    mov ax, [.remaining]
    test ax, ax
    jz .done

    ; Read up to 18 sectors at a time (stay within track)
    cmp ax, 18
    jbe .use_remaining
    mov ax, 18
.use_remaining:
    mov [.count], al

    mov ah, 0x02                ; BIOS read sectors
    mov al, [.count]
    mov ch, 0                   ; Cylinder 0
    mov cl, [.sector]           ; Sector number
    mov dh, 0                   ; Head 0
    mov dl, [boot_drive]
    int 0x13
    jc .error

    ; Update counters
    movzx ax, byte [.count]
    sub [.remaining], ax
    add [.sector], al

    ; Advance buffer (count * 512)
    mov ax, [.count]
    shl ax, 9                   ; * 512
    add bx, ax

    jmp .read_loop

.done:
    popa
    ret

.error:
    mov si, msg_disk_error
    call print_string
    jmp hang

.remaining: dw 0
.sector:    db 0
.count:     db 0

; Disk Address Packet for extended read
align 4
dap:
    db 0x10                     ; Size of DAP
    db 0                        ; Reserved
    dw STAGE2_SECTORS           ; Number of sectors
    dw STAGE2_LOAD_ADDR         ; Offset
    dw 0                        ; Segment
    dd 1                        ; LBA (start at sector 1)
    dd 0                        ; Upper 32 bits of LBA

; =============================================================================
; print_string - Print null-terminated string
; Input: SI = pointer to string
; =============================================================================
print_string:
    pusha
    mov ah, 0x0E                ; BIOS teletype function

.loop:
    lodsb                       ; Load byte from SI into AL, increment SI
    test al, al                 ; Check if null terminator
    jz .done
    int 0x10                    ; Print character
    jmp .loop

.done:
    popa
    ret

; =============================================================================
; hang - Halt the system
; =============================================================================
hang:
    cli                         ; Disable interrupts
    hlt                         ; Halt CPU
    jmp hang                    ; Just in case

; =============================================================================
; Data Section
; =============================================================================
boot_drive:     db 0

msg_loading:    db "Chanux Stage 1...", 0x0D, 0x0A, 0
msg_loaded:     db "Stage 2 loaded!", 0x0D, 0x0A, 0
msg_disk_error: db "Disk Error!", 0x0D, 0x0A, 0

; =============================================================================
; Boot Sector Padding and Signature
; =============================================================================
; Pad to 510 bytes and add boot signature
times 510 - ($ - $$) db 0
dw 0xAA55                       ; Boot signature (required by BIOS)
