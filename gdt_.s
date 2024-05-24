section .data
gdt_start:
    dq 0x0000000000000000  ; Null descriptor
    dq 0x00CF9A000000FFFF  ; Code segment descriptor
    dq 0x00CF92000000FFFF  ; Data segment descriptor
gdt_end:

gdt_pointer:
    dw gdt_end - gdt_start - 1  ; Limit (size of GDT - 1)
    dd gdt_start                ; Base address of GDT

section .text
global load_gdt ; make the label outb visible outside this file
                ; load_gdt - load the gdt
                ; stack: [esp + 4] gdt descriptor struct
                ;        [esp    ] return address
load_gdt:
    ; Load the GDT
    mov eax, [esp+4] ; Get the address of the gdt_descriptor
    lgdt [eax]       ; Load the gdt

    ; Perform a far jump to flush the prefetch queue and load the new CS
    jmp 0x08:.flush

.flush:
    ; Load data segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Return from the function
    ret