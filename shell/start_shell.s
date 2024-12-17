extern main
extern exit
section .text

call main ; execute c code, store return value in eax

call exit