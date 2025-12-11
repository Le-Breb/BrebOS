[bits 32]

section .jumpstart

global _start
extern main

; -------------------------------------------
; This code's only purpose is to call main.
; Stage1 jumps to the beginning of stage2.
; This code will be located there, allowing
; to call main indirectly.
; -------------------------------------------
_start:
    call main
    jmp $
