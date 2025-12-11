[bits 16]
[org 0x7C00] ; this is where the BIOS loads this bootloader

N_SECTORS equ 0x14 ; Number of sectors to load

STAGE2_ADDR equ  0x1000
STAGE2_SEG equ   0x0000
STAGE2_OFF equ   0x1000

FRAMEBUFFER_INFO_BUFFER_ADDR equ 0x500
FRAMEBUFFER_MODE_INFO_BUFFER_ADDR equ 0x700

FB_WIDTH            equ 1024
FB_HEIGH            equ 768
FB_BPP              equ 32
FB_COL_MASK_SIZE    equ 8
FB_RED_POS          equ 16
FB_GREEN_POS        equ 8
FB_BLUE_POS         equ 0

; -------------------------------------------
; Get  boot drive, stored in dl by the BIOS
; -------------------------------------------
mov [BOOT_DRIVE], dl

cli ; Disable interrupts

; -------------------------------------------
; Setup stack - 0x9000 is free and does not collide with anything
; -------------------------------------------
xor ax, ax
mov ss, ax
mov bp, 0x9000
mov sp, bp

; -------------------------------------------
; Enable A20 line
; -------------------------------------------
enable_a20_fast:
    in al, 0x92        ; Read port 0x92 (System Control Port A)
    or al, 00000010b   ; Set bit 1 (A20 Gate)
    and al, 0xFE       ; Clear bit 0 (System reset)
    out 0x92, al

; -------------------------------------------
; Load stage2 bootloader
; -------------------------------------------
read_from_disk:
    mov ah, 0x02         ; Read
    mov al, N_SECTORS    ; Number of secotrs to read
    xor ch, ch           ; Cylinder 0
    mov cl, 0x02         ; Sector 2 (sector 1 is boot sector)
    xor dh, dh           ; Head 0
    mov dl, [BOOT_DRIVE] ; BIOS provided drive

    ; Buffer address at ES:BX (= ES*16+BX)
    mov bx, STAGE2_SEG
    mov es, bx
    mov bx, STAGE2_OFF

    int 0x13      ; Read with BIOS interrupt
    jc disk_error ; Exit if error

    cmp al, N_SECTORS ; Compare with actual read sector count
    jne sector_error  ; Exit if error

    jmp get_framebuffer_info

disk_error:
    jmp loop

sector_error:
    jmp loop

loop:
    hlt
    jmp $

; -------------------------------------------
; Get VBE Controller Info
; -------------------------------------------
get_framebuffer_info:
    xor ax, ax
    mov es, ax
    mov di, FRAMEBUFFER_INFO_BUFFER_ADDR
    mov dword [es:di], '2EBV'   ; "VBE2" in little endian
    
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne loop       ; error

; -------------------------------------------
; Load VideoModePtr (far pointer)
; -------------------------------------------
    mov ax, [FRAMEBUFFER_INFO_BUFFER_ADDR + 0x0E]   ; segment
    mov es, ax
    mov bx, [FRAMEBUFFER_INFO_BUFFER_ADDR + 0x10]   ; offset

; -------------------------------------------
; Iterate over Video Mode List
; -------------------------------------------
set_video_mode_loop:
    mov cx, [es:bx]         ; load mode number
    cmp cx, 0xFFFF
    je loop                 ; no mode found → error

; -------------------------------------------
; Get Mode Info
; -------------------------------------------
    xor ax, ax
    mov es, ax
    mov di, FRAMEBUFFER_MODE_INFO_BUFFER_ADDR

    mov ax, 0x4F01
    int 0x10
    cmp ax, 0x004F
    jne next_mode

; -------------------------------------------
; Check specs
; -------------------------------------------
    ; width
    mov dx, [es:di + 0x12]
    cmp dx, FB_WIDTH
    jne next_mode

    ; height
    mov dx, [es:di + 0x14]
    cmp dx, FB_HEIGH
    jne next_mode

    ; bpp
    mov al, [es:di + 0x19]
    cmp al, FB_BPP
    jne next_mode
    
    ; red mask size
    mov al, [es:di + 0x1F]
    cmp al, FB_COL_MASK_SIZE
    jne next_mode

    ; red field pos
    mov al, [es:di + 0x20]
    cmp al, FB_RED_POS
    jne next_mode

    ; green mask size
    mov al, [es:di + 0x21]
    cmp al, FB_COL_MASK_SIZE
    jne next_mode

    ; green field pos
    mov al, [es:di + 0x22]
    cmp al, FB_GREEN_POS
    jne next_mode

    ; blue mask size
    mov al, [es:di + 0x23]
    cmp al, FB_COL_MASK_SIZE
    jne next_mode

    ; blue field pos
    mov al, [es:di + 0x24]
    cmp al, FB_BLUE_POS
    jne next_mode

; -------------------------------------------
; FOUND MATCHING MODE → set it!
; -------------------------------------------
found_mode:
    ; Set mode with linear framebuffer
    mov bx, cx
    or  bx, 0x4000          ; enable linear framebuffer

    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne loop                ; error setting mode

    jmp switch_to_protected_mode


next_mode:
    add bx, 2               ; next mode in list
    jmp set_video_mode_loop

; -------------------------------------------
; Global Descriptor Table
; -------------------------------------------
gdt_start:
    ; First null entry
    dq 0

; -------------------------------------------
; Code segment selector
; -------------------------------------------
gdt_code:
    dw 0xFFFF      ; Segment limit [00-15]
    dw 0x0000      ; Base address [00-15]
    db 0x0         ; Base address [16-23]
    db 10011011b   ; Flags (8 bits)
    db 11001111b   ; Flags (4 bits) + Segment limit [16-19]
    db 0x0         ; Base address [24-31]

; -------------------------------------------
; Data segment descriptor
; -------------------------------------------
gdt_data:
    dw 0xFFFF    ; Segment length, bits 0-15
    dw 0x0000    ; Base address [00-15]
    db 0x0       ; Base address [16-23]
    db 10010010b ; Flags (8 bits)
    db 11001111b ; Flags (4 bits) + Segment length, [16-19]
    db 0x0       ; Base address [24-31]

gdt_end:

; -------------------------------------------
; GDT descriptor
; -------------------------------------------
gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; size (16 bit)
    dd gdt_start ; address (32 bit)

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; -------------------------------------------
; Jump in protected mode
; -------------------------------------------
switch_to_protected_mode:
    cli                              ; Disable interrupts
    lgdt [gdt_descriptor]            ; load GDT
    mov eax, cr0                     ; Load CR0
    or eax, 0x1                      ; Set protected mode bit
    mov cr0, eax                     ; Enable protected mode
    jmp CODE_SEG:init_protected_mode ; far jump to properly execute 32 bits code

; -------------------------------------------
; Protected mode
; -------------------------------------------
[bits 32]
init_protected_mode:
    ; Set segment registers
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Setup stack
    mov ebp, 0x9000
    mov esp, ebp

    call STAGE2_ADDR ; Enter stage2 bootloader
    cli
    hlt
    jmp $ ; Endless loop if kernel ever returns


BOOT_DRIVE db 0

; -------------------------------------------
; Padding
; -------------------------------------------
times 510 - ($-$$) db 0

; -------------------------------------------
; Magic number
; -------------------------------------------
dw 0xAA55
