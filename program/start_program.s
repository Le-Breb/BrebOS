extern p_main
section .text

; push argv
; push argc
call p_main ; execute c code, store return value in eax

; program_exit syscall
mov ebx, eax  ; ret value
mov eax, 0x01 ; exit
int 0x80