extern main
extern exit
extern __cxa_finalize
section .text

; Expected stack layout. ESP shall be 0x10 bytes aligned (ABI requirement)
; [ESP + 00] - argc
; [ESP + 04] - argv
; [ESP + 08] - _init address
; [ESP + 12] - _fini address
; [ESP + 16] - _init array (null terminated)
; [ESP + ??] - _fini array (null terminated)

.Linit:
    mov ebx, esp
    add ebx, 0x8 ; skip argc and argv
.Linit_arr_loop:
    mov eax, [ebx] ; Get init function address
    test eax, eax
    jz .Linit_end ; end of array, exit loop
    call eax ; call init function
    add ebx, $4 ; proceed to next entry
    jmp .Linit_arr_loop

.Linit_end:
    xor ebx, ebx ; clear ebx
    xor eax, eax ; clear eax

.Lmain:
    call main ; execute code, store return value in eax

.Lfini:
    mov ebx, esp
    add ebx, 0x8 ; set esp to first init array entry
.Lskip_init_array_loop:
    mov eax, [ebx]
    add ebx, 0x4
    test eax, eax
    jz .Lfini_array_loop ; end of init_array
    jmp .Lskip_init_array_loop
.Lfini_array_loop:
    mov eax, [ebx] ; Get fini function address
    test eax, eax
    jz .Lexit ; end of array, exit
    call eax ; call fini function
    add ebx, $4 ; proceed to next entry
    jmp .Lfini_array_loop

.Lexit:
    sub esp, 0xc0 ; for ABI 16 bytes alignement
    push 0x00 ; instruct cxa finalize to call all destructors
    call __cxa_finalize ; call global destructors registered by __cxa_atexit. Why isn't this called by gcc routines ??
    add esp, 0x10
    call exit