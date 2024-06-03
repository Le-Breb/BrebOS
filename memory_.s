; Reload CR3. Calling this function invalidates the TLB.
global reload_cr3
reload_cr3:
    mov ecx,cr3
    mov cr3,ecx
    ret

; Set a new pdt
; [esp + 0] call function ret addr
; [esp + 4] new pdt physical address
global change_pdt
change_pdt:
    mov eax, [esp + 4]
    mov cr3, eax
    ret

; Run a program in user mode
; Doc: //https://web.archive.org/web/20220426003110/http://www.jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
; [esp +  0] = call function ret addr
; [esp +  4] = program pdt physical addr
; [esp +  8] = program eip (must be 0)
; [esp + 12] = program cs
; [esp + 16] = program eflags
; [esp + 20] = program esp (must be 0xBFFFFFFC)
; [esp + 24] = program ss
global user_mode_jump
user_mode_jump:
.change_to_process_pdt:
    mov eax, [esp + 4]
    mov cr3, eax


.set_data_segments:
    mov eax, [esp + 24]
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax

.jump:
    ;push ss
    ;mov eax, [esp + 24] // eax already contains ss
    push eax
	;push esp
    mov eax, [esp + 24]
    push eax
	;push eflags
	mov eax, [esp + 24]
	push eax
    ; push cs
    mov eax, [esp + 24]
    push eax
    ; push eip
    mov eax, [esp + 24]
    push eax

	; jump
	iret