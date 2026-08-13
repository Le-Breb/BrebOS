#include "PCI.h"

#include <stdint.h>
#include "fb.h"

PCI::Device PCI::ethernet_card = Device(-1,-1, -1);
PCI::Device PCI::xCHI = Device(-1,-1, -1);

void outl(uint16_t port, uint32_t value)
{
    asm volatile ("outl %0, %1" :: "a"(value), "Nd"(port));
}

uint32_t inl(uint16_t port)
{
    uint32_t value;
    asm volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t PCI::config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    const uint32_t address = (uint32_t)((bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

void PCI::config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    const uint32_t address = (uint32_t)((bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

uint16_t PCI::pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    return (uint16_t)((config_read_dword(bus, slot, func, offset) >> ((offset & 2) * 8)) & 0xFFFF);
}

uint16_t PCI::pciCheckVendor(uint8_t bus, uint8_t slot)
{
    uint16_t vendor = pciConfigReadWord(bus, slot, 0, 0);
    return vendor;
}

uint16_t PCI::getVendorID(uint8_t bus, uint8_t device, uint8_t function)
{
    return pciConfigReadWord(bus, device, function, 0);
}

uint8_t PCI::getHeaderType(uint8_t bus, uint8_t device, uint8_t function)
{
    return (uint8_t)(pciConfigReadWord(bus, device, function, 0x0E) & 0xFF);
}

void displayCard(const char* name)
{
    printf(" Found ");
    FB::set_fg(FB_LIGHTRED);
    printf("%s", name);
    FB::set_fg(FB_WHITE);
    printf(" Card\n");
}

void PCI::checkFunction(uint8_t bus, uint8_t device, uint8_t function)
{
    uint16_t vendorID = getVendorID(bus, device, function);
    if (vendorID == 0xFFFF) return; // No device, skip

    const uint16_t deviceID = pciConfigReadWord(bus, device, function, 2); // Read device ID
    const uint16_t classSubclass = pciConfigReadWord(bus, device, function, 0x0A);
    const uint8_t classCode = classSubclass >> 8; // PCI class code (network device class is 0x02)
    const uint8_t subclass  = classSubclass & 0xFF;
    const uint16_t revProgIF = pciConfigReadWord(bus, device, function, 0x08);
    const uint8_t progIF = revProgIF >> 8;

        // printf("%x | %x | %x \n", vendorID, deviceID, classCode);

    if (vendorID == 0x8086 && deviceID == 0x100e) { // Intel PRO/1000 e1000
        displayCard("Intel PRO/1000 Ethernet");
        if (ethernet_card.bus != (uint8_t)-1u)
            irrecoverable_error("Multiple Intel PRO/1000 Ethernet detected, don't know what to do");
        ethernet_card = Device(bus, device, function);
    }// else if (vendorID == 0x8086 && deviceID == 0x1209) {
    //    displayCard("Intel 8255x");
    //    ethernet_card = Device(bus, device, function);
    //}
    else if (vendorID == 0x1af4 && deviceID == 0x1000 && classCode == 0x02) { // Virtio Network Device
        displayCard("Virtio Network");
    } else if (classCode == 0x0C && subclass == 0x03 && progIF == 0x30) {
        if (xCHI.bus != (uint8_t)-1)
            irrecoverable_error("multiple xCHI controllers detected, don't know what to do");
        displayCard("xHCI controller");
        xCHI = Device(bus, device, function);
    }
}

void PCI::checkDevice(uint8_t bus, uint8_t device)
{
    uint8_t function = 0;
    uint16_t vendorID = getVendorID(bus, device, function);
    if (vendorID == 0xFFFF) return;
    checkFunction(bus, device, function);
    uint8_t headerType = getHeaderType(bus, device, function);
    if (headerType & 0x80) // Multi-function device, check other functions
        for (function = 1; function < 8; function++)
            if (getVendorID(bus, device, function) != 0xFFFF)
                checkFunction(bus, device, function);
}

void PCI::checkAllBuses()
{
    for (uint16_t bus = 0; bus < 256; bus++)
        for (uint8_t device = 0; device < 32; device++)
            checkDevice(bus, device);
}

uint32_t PCI::getPCIBar(uint8_t bus, uint8_t device, uint8_t function, uint8_t barIndex)
{
    uint8_t barOffset = 0x10 + (barIndex * 4); // BARs are located starting at offset 0x10
    uint32_t value = pciConfigReadWord(bus, device, function, barOffset);
    value |= (pciConfigReadWord(bus, device, function, barOffset + 2) << 16);  // Combine high and low parts for full 32-bit BAR value
    return value;
}

uint32_t PCI::getPCIBarType(uint8_t bus, uint8_t device, uint8_t function, uint8_t barIndex)
{
    uint32_t barValue = getPCIBar(bus, device, function, barIndex);
    // Check the low bit to determine if it's I/O or memory-mapped
    if (barValue & 0x1) // If the lowest bit is 1, it's an I/O space
        return PCI_BAR_IO;
    // If the lowest bit is 0, it's a memory-mapped space
    return PCI_BAR_MEM;
}

void PCI::enableBusMaster(uint8_t bus, uint8_t device, uint8_t function)
{
    // Command and Status share a dword, so read-modify-write the whole thing rather than writing a
    // zero-extended 16-bit value (which would blow away the Status half). Status bits are all
    // write-1-to-clear, so writing back exactly what we read leaves them untouched.
    uint32_t command_status = config_read_dword(bus, device, function, PCI_COMMAND);

    command_status |= PCI_COMMAND_BUS_MASTER;

    config_write_dword(bus, device, function, PCI_COMMAND, command_status);
}

void PCI::enableInterrupts(uint8_t bus, uint8_t device, uint8_t function)
{
    uint32_t command_status = config_read_dword(bus, device, function, PCI_COMMAND);

    command_status &= ~(uint32_t)PCI_COMMAND_INTERRUPT_DISABLE;

    config_write_dword(bus, device, function, PCI_COMMAND, command_status);
}

uint32_t PCI::getPCIBarSize(uint8_t bus, uint8_t device, uint8_t function, uint8_t barIndex)
{
    // Get the current BAR value (base address)
    uint32_t barValue = getPCIBar(bus, device, function, barIndex);
    uint32_t size;

    // Write 0xFFFFFFFF to the BAR to allow the device to return the size of the region
    outl(0xCF8, (uint32_t)((bus << 16) | (device << 11) | (function << 8) | (0x10 + (barIndex * 4)) | 0x80000000));
    outl(0xCFC, 0xFFFFFFFF);

    // Read back the value that shows the size of the region
    size = inl(0xCFC);

    // The size is the inverse of the returned value, plus 1
    size = (~size) + 1;

    // Now write the original BAR value back to the register
    outl(0xCF8, (uint32_t)((bus << 16) | (device << 11) | (function << 8) | (0x10 + (barIndex * 4)) | 0x80000000));
    outl(0xCFC, barValue);

    return size;
}

uint8_t PCI::getIntLine(uint8_t bus, uint8_t device, uint8_t function)
{
    // The interrupt line is located at offset 0x3C in the PCI configuration space
    uint16_t value = pciConfigReadWord(bus, device, function, PCI_INTERRUPT_LINE);

    // The interrupt line is in the lower 8 bits of the word
    return (uint8_t)(value & 0xFF);
}

uint8_t PCI::getIntPin(uint8_t bus, uint8_t device, uint8_t function)
{
    // Interrupt Pin sits in the upper byte of the same word as Interrupt Line
    return (uint8_t)((pciConfigReadWord(bus, device, function, PCI_INTERRUPT_LINE) >> 8) & 0xFF);
}
