#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

#include "interrupts.h"

/**
 * Handles a syscall
 *
 * @param cpu_state CPU state
 * @param stack_state Stack state
 * */
_Noreturn void syscall_handler(cpu_state_t* cpu_state, struct stack_state* stack_state);

#endif //INCLUDE_SYSCALLS_H
