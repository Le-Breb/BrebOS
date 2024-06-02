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

global load_tss
load_tss:
   mov ax, 0x2B      ; Load the index of our TSS structure - The index is
                     ; 0x28, as it is the 5th selector and each is 8 bytes
                     ; long, but we set the bottom two bits (making 0x2B)
                     ; so that it has an RPL of 3, not zero.
   ltr ax            ; Load 0x2B into the task state register.
   ret