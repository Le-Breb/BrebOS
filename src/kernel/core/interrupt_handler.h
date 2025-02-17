#ifndef INTERRUPT_HANDLER_H
#define INTERRUPT_HANDLER_H

#include "interrupts.h"


class Interrupt_handler {
public:
  virtual ~Interrupt_handler() = default;

  virtual void fire(cpu_state_t* cpu_state, stack_state_t* stack_state) = 0;

};



#endif //INTERRUPT_HANDLER_H
