; Reload CR3. Calling this function invalidates the TLB.
global reload_cr3_asm
reload_cr3_asm:
    mov ecx,cr3
    mov cr3,ecx
    ret

; Set a new pdt
; [esp + 0] call function ret addr
; [esp + 4] new pdt physical address
global change_pdt_asm_
change_pdt_asm_:
    mov eax, [esp + 4]
    mov cr3, eax
    ret