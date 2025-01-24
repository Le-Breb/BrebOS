global lib_main

lib_main:
    push ebp ; Save EBP
    push edi
    push esi

    mov eax, 0x7 ; Syscall index
    mov edi, [esp + 0x10] ; Symbol identifier
    mov esi, [esp + 0xc] ; Program signature
    int 0x80 ; Dynamic linker syscall

    pop esi ; Restore ESI
    pop edi ; Restore EDI
    pop ebp ; Restore EBP
    add esp, 0x8 ; Pop symbol identifier and program signature

    jmp eax ; Jump to symbol
