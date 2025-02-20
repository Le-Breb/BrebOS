#include "Network.h"

#include "ARP.h"
#include "Ethernet.h"
#include "../core/PCI.h"

E1000* Network::nic = nullptr;

class E1000;

void Network::init()
{
    PCI::checkAllBuses();
}

void Network::run()
{
    nic = new E1000(PCI::ethernet_card);
    nic->start();

    // Send a broadcast ARP request for testing purposes
    ARP::packet* broadcast_request = ARP::get_broadcast_request(nic->getMacAddress());
    const uint8_t ethernet_hdr[] = {0xff,0xff,0xff,0xff,0xff,0xff,0x52,0x53,0x00,0x12,0x34,0x56, 0x08, 0x06};
    [[maybe_unused]] auto h = (Ethernet::packet*)ethernet_hdr;
    uint8_t data[sizeof(ethernet_hdr) + sizeof(ARP::packet)];
    memcpy(data, ethernet_hdr, sizeof(ethernet_hdr));
    memcpy(data + sizeof(ethernet_hdr), broadcast_request, sizeof(*broadcast_request));
    nic->sendPacket(data, sizeof(ethernet_hdr) + sizeof(ARP::packet));
}
