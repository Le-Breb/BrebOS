#ifndef PCI_H
#define PCI_H

#include <kstdint.h>

#define PCI_BAR_MEM 0x0
#define PCI_BAR_IO  0x1


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

    static uint32_t getPCIBar(uint8_t bus, uint8_t device, uint8_t function, uint8_t barIndex);

    static uint32_t getPCIBarType(uint8_t bus, uint8_t device, uint8_t function, uint8_t barIndex);

    static void enableBusMaster(uint8_t bus, uint8_t device, uint8_t function);

    static void checkAllBuses();

    static uint32_t getPCIBarSize(uint8_t bus, uint8_t device, uint8_t function, uint8_t barIndex);

    static uint8_t getIntLine(uint8_t bus, uint8_t device, uint8_t function);
};

#endif // PCI_H
