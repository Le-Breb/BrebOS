#include "Network.h"

#include "ARP.h"
#include "DHCP.h"
#include "Ethernet.h"
#include "IPV4.h"
#include "../core/PCI.h"

E1000* Network::nic = nullptr;
uint8_t Network::ip[IPV4_ADDR_LEN] = {0, 0, 0, 0};
uint8_t Network::gateway_ip[IPV4_ADDR_LEN] = {0, 0, 0, 0};
uint8_t Network::subnet_mast[IPV4_ADDR_LEN] = {0, 0, 0, 0};
const uint8_t Network::broadcast_ip[IPV4_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff};
const uint8_t Network::broadcast_mac[MAC_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint8_t Network::mac[MAC_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; // -will be set in Network::run
uint8_t Network::null_mac[MAC_ADDR_LEN] = {};
uint8_t Network::gateway_mac[MAC_ADDR_LEN] = {};

class E1000;

void Network::init()
{
    PCI::checkAllBuses();
}

void Network::run()
{
    if (PCI::ethernet_card.bus == (uint8_t)-1)
        return; // No network card found
    nic = new E1000(PCI::ethernet_card);
    nic->start();
    memcpy(mac, nic->getMacAddress(), sizeof(mac));

    DHCP::send_discover(); // Ask for an IP address
}

void Network::send_packet(const Ethernet::packet_info* packet)
{
    // Unknown destination MAC address
    if (memcmp(&packet->packet->header.dest, null_mac, MAC_ADDR_LEN) == 0)
    {
        auto ip_header = (IPV4::header_t*)packet->packet->payload;
        if (IPV4::address_is_in_subnet((uint8_t*)&ip_header->daddr))
        {
            // Destination is within the subnet, send directly using ARP
            ARP::resolve_and_send(packet);
            return;
        }

        // Destination is outside the subnet, send to the gateway
        memcpy(&packet->packet->header.dest, gateway_mac, MAC_ADDR_LEN);
    }
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

uint32_t Network::generate_random_id32()
{
    uint64_t tsc;
    asm volatile ("rdtsc" : "=A"(tsc));
    return (uint32_t)(tsc & 0xFFFFFFFF) ^ (mac[0] << 16);
}

uint32_t Network::generate_random_id16()
{
    auto rid = generate_random_id32();
    return (rid >> 16) + (rid & 0xFF);
}

uint16_t Network::checksum_add(uint16_t checksum1, uint16_t checksum2)
{
    uint32_t sum = checksum1 + checksum2;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)sum;
}
