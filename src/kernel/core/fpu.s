; https://wiki.osdev.org/FPU

section .text
global fpu_init_asm_

fpu_init_asm_:
    ; Get CR0
    mov     eax, cr0

    ; Clear EM (bit 2) and TS (bit 3)
    and     eax, ~(1 << 2 | 1 << 3)     ; ~(0xC) = ~0x0000000C
    mov     cr0, eax

    ; Initialize FPU
    fninit

    ; Store FPU status word to memory
    fnstsw  [testword]

    ; Compare it to 0
    cmp     word [testword], 0
    jne     nofpu

hasfpu:
    ; FPU is present
    mov     eax, 1
    ret

nofpu:
    ; FPU is not present
    mov     eax, 0
    ret

section .bss
align 2
testword:    resw 1
