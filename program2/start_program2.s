extern start_main
extern exit
section .text

; push argv
; push argc
call start_main ; execute c code, store return value in eax

call exit