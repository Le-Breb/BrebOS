extern interrupt_handler

; Exit from a syscall and resume user program
; [esp + 0] = call function ret addr
; [esp + 4] = cpu_state_t
; [esp + 4 + sizeof(cpu_state_t)] = struct stack_state
global user_process_jump_asm
user_process_jump_asm:
.restore_regs:
    mov ebx, [esp + 08]
    mov ecx, [esp + 12]
    mov edx, [esp + 16]
    mov esi, [esp + 20]
    mov edi, [esp + 24]
    mov ebp, [esp + 28]

.jump:
    ;push ss
    mov eax, [esp + 52]
    push eax
	;push esp
    mov eax, [esp + 52]
    push eax
	;push eflags
	mov eax, [esp + 52]
	push eax
    ; push cs
    mov eax, [esp + 52]
    push eax
    ; push eip
    mov eax, [esp + 52]
    push eax

    mov eax, [esp + 24]

	; jump
	iret

global disable_interrupts
disable_interrupts:
    cli
    ret

global enable_interrupts
enable_interrupts:
    sti
    ret

; load_idt - Loads the interrupt descriptor table (IDT).
; stack: [esp + 4] the address of the first entry in the IDT
;        [esp    ] the return address
extern idtdbg
extern idt_dbg
global load_idt
load_idt:
    mov     eax, [esp+4]    ; load the address of the IDT into register eax
    lidt    [eax]           ; load the IDT
    ret                     ; return to the calling function

%macro no_error_code_interrupt_handler 1
global interrupt_handler_%1
interrupt_handler_%1:
    push    dword 0                     ; push 0 as error code
    push    dword %1                    ; push the interrupt number
    jmp     common_interrupt_handler    ; jump to the common handler
%endmacro

%macro error_code_interrupt_handler 1
global interrupt_handler_%1
interrupt_handler_%1:
    push    dword %1                    ; push the interrupt number
    jmp     common_interrupt_handler    ; jump to the common handler
%endmacro

%macro save_regs 0
    push    ebp
    push    edi
    push    esi
    push    edx
    push    ecx
    push    ebx
    push    eax
%endmacro

%macro restore_regs 0
    pop     eax
    pop     ebx
    pop     ecx
    pop     edx
    pop     esi
    pop     edi
    pop     ebp
%endmacro

common_interrupt_handler:               ; the common parts of the generic interrupt handler
    ; save the registers
    save_regs

    ; call the C function
    call    interrupt_handler

    ; restore the registers
    restore_regs

    ; restore the esp
    add     esp, 8

    ; return to the code that got interrupted
    iret

no_error_code_interrupt_handler 0       ; create handler for interrupt 0
no_error_code_interrupt_handler 1       ; create handler for interrupt 1
no_error_code_interrupt_handler 2       ; create handler for interrupt 2
no_error_code_interrupt_handler 3       ; create handler for interrupt 3
no_error_code_interrupt_handler 4       ; create handler for interrupt 4
no_error_code_interrupt_handler 5       ; create handler for interrupt 5
no_error_code_interrupt_handler 6       ; create handler for interrupt 6
error_code_interrupt_handler    7       ; create handler for interrupt 7
no_error_code_interrupt_handler 8       ; create handler for interrupt 8
no_error_code_interrupt_handler 9       ; create handler for interrupt 9
error_code_interrupt_handler    10      ; create handler for interrupt 10
error_code_interrupt_handler    11      ; create handler for interrupt 11
error_code_interrupt_handler    12      ; create handler for interrupt 12
error_code_interrupt_handler    13      ; create handler for interrupt 13
error_code_interrupt_handler    14      ; create handler for interrupt 14
no_error_code_interrupt_handler 15      ; create handler for interrupt 15
no_error_code_interrupt_handler 16      ; create handler for interrupt 16
no_error_code_interrupt_handler 17      ; create handler for interrupt 17
no_error_code_interrupt_handler 18      ; create handler for interrupt 18
no_error_code_interrupt_handler 19      ; create handler for interrupt 19
no_error_code_interrupt_handler 20      ; create handler for interrupt 20
no_error_code_interrupt_handler 21      ; create handler for interrupt 21
no_error_code_interrupt_handler 22      ; create handler for interrupt 22
no_error_code_interrupt_handler 23      ; create handler for interrupt 23
no_error_code_interrupt_handler 24      ; create handler for interrupt 24
no_error_code_interrupt_handler 25      ; create handler for interrupt 25
no_error_code_interrupt_handler 26      ; create handler for interrupt 26
no_error_code_interrupt_handler 27      ; create handler for interrupt 27
no_error_code_interrupt_handler 28      ; create handler for interrupt 28
no_error_code_interrupt_handler 29      ; create handler for interrupt 29
no_error_code_interrupt_handler 30      ; create handler for interrupt 30
no_error_code_interrupt_handler 31      ; create handler for interrupt 31
no_error_code_interrupt_handler 32      ; create handler for interrupt 32
no_error_code_interrupt_handler 33      ; create handler for interrupt 33
no_error_code_interrupt_handler 34      ; create handler for interrupt 34
no_error_code_interrupt_handler 35      ; create handler for interrupt 35
no_error_code_interrupt_handler 36      ; create handler for interrupt 36
no_error_code_interrupt_handler 37      ; create handler for interrupt 37
no_error_code_interrupt_handler 38      ; create handler for interrupt 38
no_error_code_interrupt_handler 39      ; create handler for interrupt 39
no_error_code_interrupt_handler 40      ; create handler for interrupt 40
no_error_code_interrupt_handler 41      ; create handler for interrupt 41
no_error_code_interrupt_handler 42      ; create handler for interrupt 42
no_error_code_interrupt_handler 43      ; create handler for interrupt 43
no_error_code_interrupt_handler 44      ; create handler for interrupt 44
no_error_code_interrupt_handler 45      ; create handler for interrupt 45
no_error_code_interrupt_handler 46      ; create handler for interrupt 46
no_error_code_interrupt_handler 47      ; create handler for interrupt 47
no_error_code_interrupt_handler 48      ; create handler for interrupt 48
no_error_code_interrupt_handler 49      ; create handler for interrupt 49
no_error_code_interrupt_handler 50      ; create handler for interrupt 50
no_error_code_interrupt_handler 51      ; create handler for interrupt 51
no_error_code_interrupt_handler 52      ; create handler for interrupt 52
no_error_code_interrupt_handler 53      ; create handler for interrupt 53
no_error_code_interrupt_handler 54      ; create handler for interrupt 54
no_error_code_interrupt_handler 55      ; create handler for interrupt 55
no_error_code_interrupt_handler 56      ; create handler for interrupt 56
no_error_code_interrupt_handler 57      ; create handler for interrupt 57
no_error_code_interrupt_handler 58      ; create handler for interrupt 58
no_error_code_interrupt_handler 59      ; create handler for interrupt 59
no_error_code_interrupt_handler 60      ; create handler for interrupt 60
no_error_code_interrupt_handler 61      ; create handler for interrupt 61
no_error_code_interrupt_handler 62      ; create handler for interrupt 62
no_error_code_interrupt_handler 63      ; create handler for interrupt 63
no_error_code_interrupt_handler 64      ; create handler for interrupt 64
no_error_code_interrupt_handler 65      ; create handler for interrupt 65
no_error_code_interrupt_handler 66      ; create handler for interrupt 66
no_error_code_interrupt_handler 67      ; create handler for interrupt 67
no_error_code_interrupt_handler 68      ; create handler for interrupt 68
no_error_code_interrupt_handler 69      ; create handler for interrupt 69
no_error_code_interrupt_handler 70      ; create handler for interrupt 70
no_error_code_interrupt_handler 71      ; create handler for interrupt 71
no_error_code_interrupt_handler 72      ; create handler for interrupt 72
no_error_code_interrupt_handler 73      ; create handler for interrupt 73
no_error_code_interrupt_handler 74      ; create handler for interrupt 74
no_error_code_interrupt_handler 75      ; create handler for interrupt 75
no_error_code_interrupt_handler 76      ; create handler for interrupt 76
no_error_code_interrupt_handler 77      ; create handler for interrupt 77
no_error_code_interrupt_handler 78      ; create handler for interrupt 78
no_error_code_interrupt_handler 79      ; create handler for interrupt 79
no_error_code_interrupt_handler 80      ; create handler for interrupt 80
no_error_code_interrupt_handler 81      ; create handler for interrupt 81
no_error_code_interrupt_handler 82      ; create handler for interrupt 82
no_error_code_interrupt_handler 83      ; create handler for interrupt 83
no_error_code_interrupt_handler 84      ; create handler for interrupt 84
no_error_code_interrupt_handler 85      ; create handler for interrupt 85
no_error_code_interrupt_handler 86      ; create handler for interrupt 86
no_error_code_interrupt_handler 87      ; create handler for interrupt 87
no_error_code_interrupt_handler 88      ; create handler for interrupt 88
no_error_code_interrupt_handler 89      ; create handler for interrupt 89
no_error_code_interrupt_handler 90      ; create handler for interrupt 90
no_error_code_interrupt_handler 91      ; create handler for interrupt 91
no_error_code_interrupt_handler 92      ; create handler for interrupt 92
no_error_code_interrupt_handler 93      ; create handler for interrupt 93
no_error_code_interrupt_handler 94      ; create handler for interrupt 94
no_error_code_interrupt_handler 95      ; create handler for interrupt 95
no_error_code_interrupt_handler 96      ; create handler for interrupt 96
no_error_code_interrupt_handler 97      ; create handler for interrupt 97
no_error_code_interrupt_handler 98      ; create handler for interrupt 98
no_error_code_interrupt_handler 99      ; create handler for interrupt 99
no_error_code_interrupt_handler 100      ; create handler for interrupt 100
no_error_code_interrupt_handler 101      ; create handler for interrupt 101
no_error_code_interrupt_handler 102      ; create handler for interrupt 102
no_error_code_interrupt_handler 103      ; create handler for interrupt 103
no_error_code_interrupt_handler 104      ; create handler for interrupt 104
no_error_code_interrupt_handler 105      ; create handler for interrupt 105
no_error_code_interrupt_handler 106      ; create handler for interrupt 106
no_error_code_interrupt_handler 107      ; create handler for interrupt 107
no_error_code_interrupt_handler 108      ; create handler for interrupt 108
no_error_code_interrupt_handler 109      ; create handler for interrupt 109
no_error_code_interrupt_handler 110      ; create handler for interrupt 110
no_error_code_interrupt_handler 111      ; create handler for interrupt 111
no_error_code_interrupt_handler 112      ; create handler for interrupt 112
no_error_code_interrupt_handler 113      ; create handler for interrupt 113
no_error_code_interrupt_handler 114      ; create handler for interrupt 114
no_error_code_interrupt_handler 115      ; create handler for interrupt 115
no_error_code_interrupt_handler 116      ; create handler for interrupt 116
no_error_code_interrupt_handler 117      ; create handler for interrupt 117
no_error_code_interrupt_handler 118      ; create handler for interrupt 118
no_error_code_interrupt_handler 119      ; create handler for interrupt 119
no_error_code_interrupt_handler 120      ; create handler for interrupt 120
no_error_code_interrupt_handler 121      ; create handler for interrupt 121
no_error_code_interrupt_handler 122      ; create handler for interrupt 122
no_error_code_interrupt_handler 123      ; create handler for interrupt 123
no_error_code_interrupt_handler 124      ; create handler for interrupt 124
no_error_code_interrupt_handler 125      ; create handler for interrupt 125
no_error_code_interrupt_handler 126      ; create handler for interrupt 126
no_error_code_interrupt_handler 127      ; create handler for interrupt 127
no_error_code_interrupt_handler 128      ; create handler for interrupt 128
no_error_code_interrupt_handler 129      ; create handler for interrupt 129
no_error_code_interrupt_handler 130      ; create handler for interrupt 130
no_error_code_interrupt_handler 131      ; create handler for interrupt 131
no_error_code_interrupt_handler 132      ; create handler for interrupt 132
no_error_code_interrupt_handler 133      ; create handler for interrupt 133
no_error_code_interrupt_handler 134      ; create handler for interrupt 134
no_error_code_interrupt_handler 135      ; create handler for interrupt 135
no_error_code_interrupt_handler 136      ; create handler for interrupt 136
no_error_code_interrupt_handler 137      ; create handler for interrupt 137
no_error_code_interrupt_handler 138      ; create handler for interrupt 138
no_error_code_interrupt_handler 139      ; create handler for interrupt 139
no_error_code_interrupt_handler 140      ; create handler for interrupt 140
no_error_code_interrupt_handler 141      ; create handler for interrupt 141
no_error_code_interrupt_handler 142      ; create handler for interrupt 142
no_error_code_interrupt_handler 143      ; create handler for interrupt 143
no_error_code_interrupt_handler 144      ; create handler for interrupt 144
no_error_code_interrupt_handler 145      ; create handler for interrupt 145
no_error_code_interrupt_handler 146      ; create handler for interrupt 146
no_error_code_interrupt_handler 147      ; create handler for interrupt 147
no_error_code_interrupt_handler 148      ; create handler for interrupt 148
no_error_code_interrupt_handler 149      ; create handler for interrupt 149
no_error_code_interrupt_handler 150      ; create handler for interrupt 150
no_error_code_interrupt_handler 151      ; create handler for interrupt 151
no_error_code_interrupt_handler 152      ; create handler for interrupt 152
no_error_code_interrupt_handler 153      ; create handler for interrupt 153
no_error_code_interrupt_handler 154      ; create handler for interrupt 154
no_error_code_interrupt_handler 155      ; create handler for interrupt 155
no_error_code_interrupt_handler 156      ; create handler for interrupt 156
no_error_code_interrupt_handler 157      ; create handler for interrupt 157
no_error_code_interrupt_handler 158      ; create handler for interrupt 158
no_error_code_interrupt_handler 159      ; create handler for interrupt 159
no_error_code_interrupt_handler 160      ; create handler for interrupt 160
no_error_code_interrupt_handler 161      ; create handler for interrupt 161
no_error_code_interrupt_handler 162      ; create handler for interrupt 162
no_error_code_interrupt_handler 163      ; create handler for interrupt 163
no_error_code_interrupt_handler 164      ; create handler for interrupt 164
no_error_code_interrupt_handler 165      ; create handler for interrupt 165
no_error_code_interrupt_handler 166      ; create handler for interrupt 166
no_error_code_interrupt_handler 167      ; create handler for interrupt 167
no_error_code_interrupt_handler 168      ; create handler for interrupt 168
no_error_code_interrupt_handler 169      ; create handler for interrupt 169
no_error_code_interrupt_handler 170      ; create handler for interrupt 170
no_error_code_interrupt_handler 171      ; create handler for interrupt 171
no_error_code_interrupt_handler 172      ; create handler for interrupt 172
no_error_code_interrupt_handler 173      ; create handler for interrupt 173
no_error_code_interrupt_handler 174      ; create handler for interrupt 174
no_error_code_interrupt_handler 175      ; create handler for interrupt 175
no_error_code_interrupt_handler 176      ; create handler for interrupt 176
no_error_code_interrupt_handler 177      ; create handler for interrupt 177
no_error_code_interrupt_handler 178      ; create handler for interrupt 178
no_error_code_interrupt_handler 179      ; create handler for interrupt 179
no_error_code_interrupt_handler 180      ; create handler for interrupt 180
no_error_code_interrupt_handler 181      ; create handler for interrupt 181
no_error_code_interrupt_handler 182      ; create handler for interrupt 182
no_error_code_interrupt_handler 183      ; create handler for interrupt 183
no_error_code_interrupt_handler 184      ; create handler for interrupt 184
no_error_code_interrupt_handler 185      ; create handler for interrupt 185
no_error_code_interrupt_handler 186      ; create handler for interrupt 186
no_error_code_interrupt_handler 187      ; create handler for interrupt 187
no_error_code_interrupt_handler 188      ; create handler for interrupt 188
no_error_code_interrupt_handler 189      ; create handler for interrupt 189
no_error_code_interrupt_handler 190      ; create handler for interrupt 190
no_error_code_interrupt_handler 191      ; create handler for interrupt 191
no_error_code_interrupt_handler 192      ; create handler for interrupt 192
no_error_code_interrupt_handler 193      ; create handler for interrupt 193
no_error_code_interrupt_handler 194      ; create handler for interrupt 194
no_error_code_interrupt_handler 195      ; create handler for interrupt 195
no_error_code_interrupt_handler 196      ; create handler for interrupt 196
no_error_code_interrupt_handler 197      ; create handler for interrupt 197
no_error_code_interrupt_handler 198      ; create handler for interrupt 198
no_error_code_interrupt_handler 199      ; create handler for interrupt 199
no_error_code_interrupt_handler 200      ; create handler for interrupt 200
no_error_code_interrupt_handler 201      ; create handler for interrupt 201
no_error_code_interrupt_handler 202      ; create handler for interrupt 202
no_error_code_interrupt_handler 203      ; create handler for interrupt 203
no_error_code_interrupt_handler 204      ; create handler for interrupt 204
no_error_code_interrupt_handler 205      ; create handler for interrupt 205
no_error_code_interrupt_handler 206      ; create handler for interrupt 206
no_error_code_interrupt_handler 207      ; create handler for interrupt 207
no_error_code_interrupt_handler 208      ; create handler for interrupt 208
no_error_code_interrupt_handler 209      ; create handler for interrupt 209
no_error_code_interrupt_handler 210      ; create handler for interrupt 210
no_error_code_interrupt_handler 211      ; create handler for interrupt 211
no_error_code_interrupt_handler 212      ; create handler for interrupt 212
no_error_code_interrupt_handler 213      ; create handler for interrupt 213
no_error_code_interrupt_handler 214      ; create handler for interrupt 214
no_error_code_interrupt_handler 215      ; create handler for interrupt 215
no_error_code_interrupt_handler 216      ; create handler for interrupt 216
no_error_code_interrupt_handler 217      ; create handler for interrupt 217
no_error_code_interrupt_handler 218      ; create handler for interrupt 218
no_error_code_interrupt_handler 219      ; create handler for interrupt 219
no_error_code_interrupt_handler 220      ; create handler for interrupt 220
no_error_code_interrupt_handler 221      ; create handler for interrupt 221
no_error_code_interrupt_handler 222      ; create handler for interrupt 222
no_error_code_interrupt_handler 223      ; create handler for interrupt 223
no_error_code_interrupt_handler 224      ; create handler for interrupt 224
no_error_code_interrupt_handler 225      ; create handler for interrupt 225
no_error_code_interrupt_handler 226      ; create handler for interrupt 226
no_error_code_interrupt_handler 227      ; create handler for interrupt 227
no_error_code_interrupt_handler 228      ; create handler for interrupt 228
no_error_code_interrupt_handler 229      ; create handler for interrupt 229
no_error_code_interrupt_handler 230      ; create handler for interrupt 230
no_error_code_interrupt_handler 231      ; create handler for interrupt 231
no_error_code_interrupt_handler 232      ; create handler for interrupt 232
no_error_code_interrupt_handler 233      ; create handler for interrupt 233
no_error_code_interrupt_handler 234      ; create handler for interrupt 234
no_error_code_interrupt_handler 235      ; create handler for interrupt 235
no_error_code_interrupt_handler 236      ; create handler for interrupt 236
no_error_code_interrupt_handler 237      ; create handler for interrupt 237
no_error_code_interrupt_handler 238      ; create handler for interrupt 238
no_error_code_interrupt_handler 239      ; create handler for interrupt 239
no_error_code_interrupt_handler 240      ; create handler for interrupt 240
no_error_code_interrupt_handler 241      ; create handler for interrupt 241
no_error_code_interrupt_handler 242      ; create handler for interrupt 242
no_error_code_interrupt_handler 243      ; create handler for interrupt 243
no_error_code_interrupt_handler 244      ; create handler for interrupt 244
no_error_code_interrupt_handler 245      ; create handler for interrupt 245
no_error_code_interrupt_handler 246      ; create handler for interrupt 246
no_error_code_interrupt_handler 247      ; create handler for interrupt 247
no_error_code_interrupt_handler 248      ; create handler for interrupt 248
no_error_code_interrupt_handler 249      ; create handler for interrupt 249
no_error_code_interrupt_handler 250      ; create handler for interrupt 250
no_error_code_interrupt_handler 251      ; create handler for interrupt 251
no_error_code_interrupt_handler 252      ; create handler for interrupt 252
no_error_code_interrupt_handler 253      ; create handler for interrupt 253
no_error_code_interrupt_handler 254      ; create handler for interrupt 254
no_error_code_interrupt_handler 255      ; create handler for interrupt 255