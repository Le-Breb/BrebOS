extern main
extern exit
section .text

mov ebp, esp
add ebp, 8
call main ; execute c code, store return value in eax
add esp, 8

call exit