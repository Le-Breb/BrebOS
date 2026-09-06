#ifndef INTERRUPT_HANDLER_H
#define INTERRUPT_HANDLER_H

#include "interrupts.h"


class Interrupt_handler {
public:
  virtual ~Interrupt_handler() = default;

  /**
   * Handles the interrupt if it was raised by this device.
   *
   * The underlying IRQ line may be shared by several PCI devices (legacy INTx routing), so this
   * gets called for every handler registered on the vector. Implementations must check their own
   * device status register and return false without side effects if they weren't the source.
   *
   * @return true if this device raised the interrupt and it was handled, false otherwise
   */
  virtual bool fire(cpu_state_t* cpu_state, stack_state_t* stack_state) = 0;

};



#endif //INTERRUPT_HANDLER_H
