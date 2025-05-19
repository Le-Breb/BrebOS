global switch_stack_and_call_process_function

switch_stack_and_call_process_function:
    mov eax, [esp + 8] ; get the address of the function to call
    mov ebx, [esp + 12] ; get the function parameter (ie process pointer)
    mov esp, [esp + 4] ; set the new stack pointer
    push ebx ; push the function parameter
    call eax ; call the function