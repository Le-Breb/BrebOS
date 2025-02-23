#include "E1000.h"

#include <kstring.h>

#include "ARP.h"
#include "Endianness.h"
#include "Ethernet.h"
#include "Network.h"
#include "ports.h"
#include "../core/memory.h"
#include "../core/fb.h"
#include "../core/interrupts.h"
#include "../core/PIC.h"

uint8_t MMIOUtils::read8(uint32_t p_address)
{
    return *((volatile uint8_t*)(p_address));
}

uint16_t MMIOUtils::read16(uint32_t p_address)
{
    return *((volatile uint16_t*)(p_address));
}

uint32_t MMIOUtils::read32(uint32_t p_address)
{
    return *((volatile uint32_t*)(p_address));
}

uint32_t MMIOUtils::read64(uint32_t p_address)
{
    return *((volatile uint32_t*)(p_address));
}

void MMIOUtils::write8(uint32_t p_address, uint8_t p_value)
{
    (*((volatile uint8_t*)(p_address))) = (p_value);
}

void MMIOUtils::write16(uint32_t p_address, uint16_t p_value)
{
    (*((volatile uint16_t*)(p_address))) = (p_value);
}

void MMIOUtils::write32(uint32_t p_address, uint32_t p_value)
{
    (*((volatile uint32_t*)(p_address))) = (p_value);
}

/*void MMIOUtils::write64(uint64_t p_address, uint64_t p_value)
{
    (*((volatile uint64_t*)(p_address))) = (p_value);
}*/

void E1000::writeCommand(uint16_t p_address, uint32_t p_value) const
{
    if (bar_type == 0)
    {
        MMIOUtils::write32(mem_base + p_address, p_value);
    }
    else
    {
        Ports::outportl(io_base, p_address);
        Ports::outportl(io_base + 4, p_value);
    }
}

uint32_t E1000::readCommand(uint16_t p_address) const
{
    if (bar_type == 0)
    {
        return MMIOUtils::read32(mem_base + p_address);
    }
    else
    {
        Ports::outportl(io_base, p_address);
        return Ports::inportl(io_base + 4);
    }
}

bool E1000::detectEEProm()
{
    uint32_t val = 0;
    writeCommand(REG_EEPROM, 0x1);

    for (int i = 0; i < 1000 && !eerprom_exists; i++)
    {
        val = readCommand(REG_EEPROM);
        if (val & 0x10)
            eerprom_exists = true;
        else
            eerprom_exists = false;
    }
    return eerprom_exists;
}

uint32_t E1000::eepromRead(uint8_t addr) const
{
    uint16_t data = 0;
    uint32_t tmp = 0;
    if (eerprom_exists)
    {
        writeCommand(REG_EEPROM, (1) | ((uint32_t)(addr) << 8));
        while (!((tmp = readCommand(REG_EEPROM)) & (1 << 4)));
    }
    else
    {
        writeCommand(REG_EEPROM, (1) | ((uint32_t)(addr) << 2));
        while (!((tmp = readCommand(REG_EEPROM)) & (1 << 1)));
    }
    data = (uint16_t)((tmp >> 16) & 0xFFFF);
    return data;
}

bool E1000::readMACAddress()
{
    if (eerprom_exists)
    {
        uint32_t temp;
        temp = eepromRead(0);
        mac[0] = temp & 0xff;
        mac[1] = temp >> 8;
        temp = eepromRead(1);
        mac[2] = temp & 0xff;
        mac[3] = temp >> 8;
        temp = eepromRead(2);
        mac[4] = temp & 0xff;
        mac[5] = temp >> 8;
    }
    else
    {
        uint8_t* mem_base_mac_8 = (uint8_t*)(mem_base + 0x5400);
        uint32_t* mem_base_mac_32 = (uint32_t*)(mem_base + 0x5400);
        if (mem_base_mac_32[0] != 0)
        {
            for (int i = 0; i < 6; i++)
            {
                mac[i] = mem_base_mac_8[i];
            }
        }
        else return false;
    }
    return true;
}

void E1000::rxinit()
{
    uint8_t* ptr;
    struct e1000_rx_desc* descs;

    // Allocate buffer for receive descriptors. For simplicity, in my case khmalloc returns a virtual address that is identical to it physical mapped address.
    // In your case you should handle virtual and physical addresses as the addresses passed to the NIC should be physical ones

    ptr = (uint8_t*)malloc(sizeof(struct e1000_rx_desc) * E1000_NUM_RX_DESC + 16);
    memset(ptr, 0, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc) + 16);
    ptr = (uint8_t*)(((uint)ptr + 15) & ~0xF); // 16 bits alignment, DMA requirement

    descs = (struct e1000_rx_desc*)ptr;
    for (int i = 0; i < E1000_NUM_RX_DESC; i++)
    {
        rx_descs[i] = (struct e1000_rx_desc*)((uint8_t*)descs + i * 16);
        uint buf = ((uint)calloc(8192 + 16, 1) + 15) & ~0xF;
        rx_desc_virt_addresses[i] = (uint8_t*)buf;
        //printf("%x | %x\n", buf, PHYS_ADDR(get_page_tables(), buf));
        rx_descs[i]->addr = PHYS_ADDR(get_page_tables(), buf);
        rx_descs[i]->status = 0;
    }

    uint32_t phys_ptr = PHYS_ADDR(get_page_tables(), (uint)ptr);

    writeCommand(REG_RXDESCLO, phys_ptr);
    writeCommand(REG_RXDESCHI, 0);

    writeCommand(REG_RXDESCLEN, E1000_NUM_RX_DESC * 16);

    writeCommand(REG_RXDESCHEAD, 0);
    writeCommand(REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
    rx_cur = 0;
    writeCommand(
        REG_RCTRL,
        RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_LBM_NONE | RTCL_RDMTS_HALF | RCTL_BAM | RCTL_SECRC |
        RCTL_BSIZE_8192);
}


void E1000::txinit()
{
    uint8_t* ptr;
    struct e1000_tx_desc* descs;
    // Allocate buffer for transmit descriptors. For simplicity, in my case khmalloc returns a virtual address that is identical to it physical mapped address.
    // In your case you should handle virtual and physical addresses as the addresses passed to the NIC should be physical ones
    ptr = (uint8_t*)malloc(sizeof(struct e1000_tx_desc) * E1000_NUM_TX_DESC + 16);
    ptr = (uint8_t*)(((uint)ptr + 15) & ~0xF); // 16 bits alignment, DMA requirement

    descs = (struct e1000_tx_desc*)ptr;
    for (int i = 0; i < E1000_NUM_TX_DESC; i++)
    {
        tx_descs[i] = (struct e1000_tx_desc*)((uint8_t*)descs + i * 16);
        memset(tx_descs[i], 0, sizeof(struct e1000_tx_desc));
    }

    uint32_t phys_ptr = PHYS_ADDR(get_page_tables(), (uint)ptr);
    writeCommand(REG_TXDESCLO, phys_ptr);
    writeCommand(REG_TXDESCHI, 0);


    //now setup total length of descriptors
    writeCommand(REG_TXDESCLEN, E1000_NUM_TX_DESC * 16);


    //setup numbers
    writeCommand(REG_TXDESCHEAD, 0);
    writeCommand(REG_TXDESCTAIL, 0);
    tx_cur = 0;
    writeCommand(REG_TCTRL, TCTL_EN
                 | TCTL_PSP
                 | (15 << TCTL_CT_SHIFT)
                 | (64 << TCTL_COLD_SHIFT)
                 | TCTL_RTLC);

    // This line of code overrides the one before it but I left both to highlight that the previous one works with e1000 cards, but for the e1000e cards
    // you should set the TCTRL register as follows. For detailed description of each bit, please refer to the Intel Manual.
    // In the case of I217 and 82577LM packets will not be sent if the TCTRL is not configured using the following bits.
    writeCommand(REG_TCTRL, 0b0110000000000111111000011111010);
    writeCommand(REG_TIPG, 0x0060200A);
}

void E1000::enableInterrupt() const
{
    writeCommand(REG_IMASK, 0x1F6DC);
    writeCommand(REG_IMASK, 0xff & ~4);
    [[maybe_unused]] auto _ = readCommand(0xc0); // acknowledge interrupt
}


E1000::E1000(PCI::Device pci_device) : device(pci_device)
{
    // Get BAR0 type, io_base address and MMIO base address
    bar_type = PCI::getPCIBarType(device.bus, device.device, device.function, 0);
    if (bar_type == PCI_BAR_IO)
        printf_info("E1000 card uses io space, this behaviour hasn't been tested");

    io_base = PCI::getPCIBar(device.bus, device.device, device.function, PCI_BAR_IO) & ~1;
    mem_base = PCI::getPCIBar(device.bus, device.device, device.function, PCI_BAR_MEM) & ~3;

    uint32_t bar_size = PCI::getPCIBarSize(device.bus, device.device, device.function, bar_type);
    if (!identity_map(mem_base, bar_size))
        printf_error("Couldn't identity E1000 MMIO address space");

    // Enable bus mastering
    PCI::enableBusMaster(device.bus, device.device, device.function);
    eerprom_exists = false;
}

bool E1000::start()
{
    detectEEProm();
    if (!readMACAddress()) return false;
    printf_info("MAC Address: %x:%x:%x:%x:%x:%x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    //startLink();

    for (int i = 0; i < 0x80; i++)
        writeCommand(0x5200 + i * 4, 0);
    if (Interrupts::register_interrupt(
        PIC1_START_INTERRUPT + PCI::getIntLine(device.bus, device.device, device.function), this))
    {
        enableInterrupt();
        rxinit();
        txinit();
        printf_info("E1000 card started");
        return true;
    }

    printf_error("Couldn't register E1000 interrupt");
    return false;
}

void E1000::fire([[maybe_unused]] cpu_state_t* cpu_state, [[maybe_unused]] stack_state_t* stack_state)
{
    /* This might be needed here if your handler doesn't clear interrupts from each device and must be done before EOI if using the PIC.
           Without this, the card will spam interrupts as the int-line will stay high. */
    // writeCommand(REG_IMASK, 0x1); // as we are writing back the status to ICR and calling PIC::acknowledge later,
    // this does not appear to be required

    const uint32_t status = readCommand(0xc0);
    writeCommand(0xC0, status); // Write the value back to clear the interrupts
    //printf_info("e1000 int: status: 0x%02X", status);

    if (status & 0x1)
    {
        tx_free();
    }
    if (status & 0x02)
    {
        //printf_info("E1000 empty queue");
    } // Transmit queue empty, will be triggerred when initializing tx descriptors
    else if (status & 0x04)
    {
        printf_info("E1000 link status changed");
    }
    else if (status & 0x10)
    {
        // good threshold
        //printf_info("E1000 good threshold");
        handleReceive();
    }
    else if (status & 0x80)
    {
        //printf_info("Receive done");
        handleReceive();
    }
    else
    {
        printf_error("Unknown E1000 interrupt status: %u", status);
        uint32_t status2 = readCommand(REG_STATUS);
        uint32_t txctl = readCommand(REG_TCTRL);
        printf_error("NIC status: %x, TCTRL: %x", status2, txctl);
    }
}

uint8_t* E1000::getMacAddress()
{
    return mac;
}

void E1000::handleReceive()
{
    while (rx_descs[rx_cur]->status & TSTA_DD)
    {
        uint8_t* buf = rx_desc_virt_addresses[rx_cur];
        uint16_t len = rx_descs[rx_cur]->length;

        Ethernet::packet_info packet_info((Ethernet::packet_t*)buf, len);
        Ethernet::handle_packet(&packet_info);

        rx_descs[rx_cur]->status = 0;
        writeCommand(REG_RXDESCTAIL, rx_cur);
        rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
    }
}

void E1000::tx_free()
{
    for (int i = 0; i < E1000_NUM_TX_DESC; i++)
    {
        if (!(tx_descs[i]->status & TSTA_DD) || tx_descs[i]->addr == 0)
            continue;
        free(tx_desc_virt_addresses[i]);
        tx_descs[i]->addr = 0;
        tx_desc_virt_addresses[i] = nullptr;
    }
}

int E1000::sendPacket(Ethernet::packet_info* packet)
{
    tx_desc_virt_addresses[tx_cur] = (uint8_t*)packet->packet;
    tx_descs[tx_cur]->addr = PHYS_ADDR(get_page_tables(), (uint32_t)packet->packet);
    tx_descs[tx_cur]->length = packet->size;
    tx_descs[tx_cur]->cmd = CMD_EOP | /*CMD_IFCS |*/ CMD_RS;
    // Todo: check if IFCS (for checksum) should be enabled back
    tx_descs[tx_cur]->status = 0;
    uint8_t old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;

    writeCommand(REG_TXDESCTAIL, tx_cur);


    while (!(tx_descs[old_cur]->status & 0xff))
    {
        // printf("TX status: 0x%x\n", tx_descs[old_cur]->status);
    };
    return 0;
}
