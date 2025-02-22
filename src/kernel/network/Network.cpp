#include "Network.h"

#include "ARP.h"
#include "Ethernet.h"
#include "../core/PCI.h"

E1000* Network::nic = nullptr;
const uint8_t Network::ip[IPV4_ADDR_LEN] = {192, 168, 100, 2};
const uint8_t Network::broadcast_ip[IPV4_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff};
const uint8_t Network::broadcast_mac[MAC_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint8_t Network::mac[MAC_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; // -will be set in Network::run

class E1000;

void Network::init()
{
    PCI::checkAllBuses();
}

void Network::run()
{
    nic = new E1000(PCI::ethernet_card);
    nic->start();
    memcpy(mac, nic->getMacAddress(), sizeof(mac));
}

void Network::sendPacket(Ethernet::packet_info* packet)
{
    nic->sendPacket(packet);
}

uint16_t Network::checksum(const void* addr, size_t count)
{
    uint32_t sum = 0;
    auto* ptr = (uint16_t*)addr;

    while (count > 1)
    {
        /*  This is the inner loop */
        sum += *ptr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if (count > 0)
        sum += *(uint8_t*)ptr;

    /*  Fold 32-bit sum to 16 bits */
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}