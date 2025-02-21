#include "Network.h"

#include "ARP.h"
#include "Ethernet.h"
#include "../core/PCI.h"

E1000* Network::nic = nullptr;
const uint8_t Network::ip[NETWORK_IPV4_LEN] = {192, 168, 100, 2};
const uint8_t Network::broadcast_ip[NETWORK_IPV4_LEN] = {0xff, 0xff, 0xff, 0xff};
const uint8_t Network::broadcast_mac[NETWORK_MAC_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint8_t Network::mac[NETWORK_MAC_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; // -will be set in Network::run

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

    // Send a broadcast ARP request for testing purposes
    Ethernet::packet_info* broadcast_request = ARP::new_broadcast_request(nic->getMacAddress());
    nic->sendPacket(broadcast_request);
}
