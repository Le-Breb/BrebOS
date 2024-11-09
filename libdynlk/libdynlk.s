global lib_main

lib_main:
    push ebp ; Save EBP
    push ebx ; Save EBX (ABI requirement)

    mov eax, 0x7 ; Syscall index
    mov ebx, [esp + 0xc] ; Symbol identifier
    mov ecx, [esp + 0x8] ; Program signature
    int 0x80 ; Dynamic linker syscall

    pop ebx ; Restore EBX
    pop ebp ; Restore EBP
    add esp, 0x8 ; Pop symbol identifier and program signature

    jmp eax ; Jump to symbol
