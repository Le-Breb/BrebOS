global loader                   ; the entry symbol for ELF
extern kmain
extern _kernel_start
extern _kernel_end

KERNEL_STACK_SIZE equ 4096          ; size of stack in bytes

MAGIC_NUMBER    equ 0xE85250D6      ; define the magic number constant
I386            equ 0x00000000      ; i386 architecture

; calculate the checksum (all options + checksum should equal 0)
CHECKSUM        equ -(MAGIC_NUMBER + 0 + (header_end - header_start))

KERNEL_VIRTUAL_BASE equ 0xC0000000  ; the virtual address where the kernel is loaded

VGA_TEXT_BUFFER equ 0x000B8000      ; the physical address of the VGA text buffer

; Declare a multiboot header that marks the program as a kernel.
section .multiboot2
align 8

    ; === Multiboot2 magic header ===
    dd MAGIC_NUMBER
    dd I386
    dd header_end - header_start
    dd CHECKSUM

align 8
header_start:

;    ; === Framebuffer tag ===
;    dw 5                      ; type = framebuffer
;    dw 0                      ; flags
;    dd 20                     ; size of this tag (must be 20 bytes)
;    dd 1024                   ; width
;    dd 768                    ; height
;    dd 32                     ; depth (bits per pixel)

align 8

    ; === End tag ===
    dw 0                      ; type = end tag
    dw 0                      ; flags
    dd 8                      ; size of end tag (must be 8 bytes)

header_end:


; Preallocate pages used for paging. Don't hard-code addresses and assume they
; are available, as the bootloader might have loaded its multiboot structures or
; modules there. This lets the bootloader know it must avoid the addresses.
section .bss
align 4096
global boot_page_directory
boot_page_directory:
    resb 4096
global boot_page_table1
boot_page_table1:
    resb 4096

; Allocate the initial stack.
align 4
stack_bottom:
    resb 16384   ; 16 KiB
global stack_top
stack_top:
; Further page tables may be required if the kernel grows beyond 3 MiB.

; The kernel entry point.
section .text
global _start
_start:
    ; Physical address of boot_page_table1.
    mov edi, boot_page_table1 - KERNEL_VIRTUAL_BASE

    ; First address to map is address 0.
    mov esi, 0

    ; Map 1023 pages. The 1024th will be the VGA text buffer.
    mov ecx, 1023

map_pages:
    ; Only map the kernel.
    cmp esi, _kernel_start
    jl skip_page
    cmp esi, _kernel_end - KERNEL_VIRTUAL_BASE
    jge done_mapping

    ; Map physical address as "present, writable". Note that this maps
    ; .text and .rodata as writable. Mind security and map them as non-writable.
    mov edx, esi
    or edx, 0x003
    mov [edi], edx

skip_page:
    ; Size of page is 4096 bytes.
    add esi, 4096
    ; Size of entries in boot_page_table1 is 4 bytes.
    add edi, 4
    ; Loop to the next entry if we haven't finished.
    loop map_pages

done_mapping:
    ; Map VGA video memory to 0xC03FF000 as "present, writable".
    mov dword [boot_page_table1 - KERNEL_VIRTUAL_BASE + 1023 * 4], VGA_TEXT_BUFFER | 0x003

    ; The page table is used at both page directory entry 0 (virtually from 0x0
    ; to 0x3FFFFF) (thus identity mapping the kernel) and page directory entry
    ; 768 (virtually from 0xC0000000 to 0xC03FFFFF) (thus mapping it in the
    ; higher half). The kernel is identity mapped because enabling paging does
    ; not change the next instruction, which continues to be physical. The CPU
    ; would instead page fault if there was no identity mapping.

    ; Map the page table to both virtual addresses 0x00000000 and 0xC0000000.
    mov eax, boot_page_table1 - KERNEL_VIRTUAL_BASE + 0x003
    mov [boot_page_directory - KERNEL_VIRTUAL_BASE + 0], eax
    mov [boot_page_directory - KERNEL_VIRTUAL_BASE + 768 * 4], eax ; 768 is 0xC0000000 / 4MB

    ; Set cr3 to the address of the boot_page_directory.
    mov ecx, boot_page_directory - KERNEL_VIRTUAL_BASE
    mov cr3, ecx

    ; Enable paging and the write-protect bit.
    mov ecx, cr0
    or ecx, 0x80010000
    mov cr0, ecx

    ; Jump to higher half with an absolute jump.
    lea ecx, [rel higher_half]
    jmp ecx

section .higher_half
higher_half:
    ; At this point, paging is fully set up and enabled.

    ; Unmap the identity mapping as it is now unnecessary.
    mov dword [boot_page_directory + 0], 0

    ; Reload cr3 to force a TLB flush so the changes to take effect.
    mov ecx, cr3
    mov cr3, ecx

    ; Set up the stack.
    mov esp, stack_top

    ; Enter the high-level kernel.
    push ebx  ; save module structure pointer
    call kmain

    ; Infinite loop if the system has nothing more to do.
    cli

halt_loop:
    hlt
    jmp halt_loop