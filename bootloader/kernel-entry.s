[bits 32]

section .jumpstart

global _start
extern main

_start:
    call main
    jmp $
