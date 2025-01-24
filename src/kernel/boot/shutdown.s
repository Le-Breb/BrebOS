; Checks wether the OS is running in QEMU
; Returns 1 if true, 0 else
global is_running_in_qemu_asm
is_running_in_qemu_asm:
    mov eax, 0x40000000

    ; cpuid has weird effects, so registers are saved then restored to not affect anything
    push ecx
    push edx
    push ebx
    push esp
    push ebp
    push esi
    push edi
    cpuid
    pop edi
    pop esi
    pop ebp
    pop esp
    pop ebx
    pop edx
    pop ecx

    cmp eax, 0x40000001
    je qemu_detected
    mov eax, 0  ; Not QEMU
    ret

qemu_detected:
    mov eax, 1  ; QEMU detected
    ret