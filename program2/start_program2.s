extern p_main
extern exit
section .text

; push argv
; push argc
call p_main ; execute c code, store return value in eax

call exit