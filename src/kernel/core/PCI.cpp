#include "PCI.h"

#include <kstdint.h>
#include "fb.h"

PCI::Device PCI::ethernet_card = Device(-1,-1, -1);

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

uint16_t PCI::pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    return (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
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
    printf("Found ");
    FB::set_fg(FB_LIGHTRED);
    printf("%s", name);
    FB::set_fg(FB_WHITE);
    printf(" Card\n");
}

void PCI::checkFunction(uint8_t bus, uint8_t device, uint8_t function)
{
        uint16_t vendorID = getVendorID(bus, device, function);
        if (vendorID == 0xFFFF) return; // No device, skip

        uint16_t deviceID = pciConfigReadWord(bus, device, function, 2); // Read device ID
        uint8_t classCode = pciConfigReadWord(bus, device, function, 0x0A) >> 8; // PCI class code (network device class is 0x02)

        // printf("%x | %x | %x \n", vendorID, deviceID, classCode);

    if (vendorID == 0x8086 && deviceID == 0x100e) { // Intel PRO/1000 e1000
        displayCard("Intel PRO/1000 Ethernet");
        ethernet_card = Device(bus, device, function);
    }// else if (vendorID == 0x8086 && deviceID == 0x1209) {
    //    displayCard("Intel 8255x");
    //    ethernet_card = Device(bus, device, function);
    //}
    else if (vendorID == 0x1af4 && deviceID == 0x1000 && classCode == 0x02) { // Virtio Network Device
        displayCard("Virtio Network");
    } else {
        // Not an Ethernet card, you can add other devices' checks here if needed.
    }
}

void PCI::checkDevice(uint8_t bus, uint8_t device)
{
    uint8_t function = 0;
    uint16_t vendorID = getVendorID(bus, device, function);
    if (vendorID == 0xFFFF) return;
    checkFunction(bus, device, function);
    uint8_t headerType = getHeaderType(bus, device, function);
    if (headerType & 0x80)
    {
        for (function = 1; function < 8; function++)
        {
            if (getVendorID(bus, device, function) != 0xFFFF)
            {
                checkFunction(bus, device, function);
            }
        }
    }
}

void PCI::checkAllBuses()
{
    for (uint16_t bus = 0; bus < 256; bus++)
    {
        for (uint8_t device = 0; device < 32; device++)
        {
            checkDevice(bus, device);
        }
    }
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
    // Read the PCI command register (offset 0x04)
    uint16_t command = pciConfigReadWord(bus, device, function, 0x04);

    // Set the bus mastering bit (bit 2)
    command |= 0x04;

    // Write the updated value back to the command register
    outl(0xCF8, (uint32_t)((bus << 16) | (device << 11) | (function << 8) | 0x04 | 0x80000000));
    outl(0xCFC, command);
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
    uint16_t value = pciConfigReadWord(bus, device, function, 0x3C);

    // The interrupt line is in the lower 8 bits of the word
    return (uint8_t)(value & 0xFF);
}