#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_BAR_MEM 0x0
#define PCI_BAR_IO  0x1

// Header offsets we touch by name rather than magic number
#define PCI_COMMAND         0x04
#define PCI_INTERRUPT_LINE  0x3C

// Command register bits
#define PCI_COMMAND_BUS_MASTER      (1 << 2)
// Set by firmware on plenty of real machines. While it is set the function never asserts its
// legacy INTx line, so its interrupts silently never reach the PIC.
#define PCI_COMMAND_INTERRUPT_DISABLE (1 << 10)


class PCI
{
    static uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

    static uint16_t pciCheckVendor(uint8_t bus, uint8_t slot);

    static uint16_t getVendorID(uint8_t bus, uint8_t device, uint8_t function);

    static uint8_t getHeaderType(uint8_t bus, uint8_t device, uint8_t function);

    static void checkFunction(uint8_t bus, uint8_t device, uint8_t function);

    static void checkDevice(uint8_t bus, uint8_t device);

public:
    struct Device
    {
        uint8_t bus, device, function;

        Device(uint8_t bus, uint8_t device, uint8_t function)
            : bus(bus),
              device(device),
              function(function)
        {
        }
    };

    static Device ethernet_card;

    static Device xCHI;

    static uint32_t getPCIBar(uint8_t bus, uint8_t device, uint8_t function, uint8_t barIndex);

    static uint32_t getPCIBarType(uint8_t bus, uint8_t device, uint8_t function, uint8_t barIndex);

    static void enableBusMaster(uint8_t bus, uint8_t device, uint8_t function);

    static void checkAllBuses();

    static uint32_t getPCIBarSize(uint8_t bus, uint8_t device, uint8_t function, uint8_t barIndex);

    static uint8_t getIntLine(uint8_t bus, uint8_t device, uint8_t function);

    /** Which INTx pin the function is wired to: 0 = none, 1 = INTA, 2 = INTB, 3 = INTC, 4 = INTD */
    static uint8_t getIntPin(uint8_t bus, uint8_t device, uint8_t function);

    static uint32_t config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

    static void config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

    /**
     * Clears the Command register's Interrupt Disable bit so the function is allowed to assert
     * its legacy INTx line. Firmware often leaves this bit set (it hands the OS a quiesced
     * device, expecting the OS to enable either INTx or MSI itself), and while it is set no
     * interrupt from this function ever reaches the PIC.
     */
    static void enableInterrupts(uint8_t bus, uint8_t device, uint8_t function);
};

#endif // PCI_H
