#ifndef NETWORK_H
#define NETWORK_H
#include "E1000.h"
#include "NetworkConsts.h"

class Network
{
    static E1000* nic;

public:
    static const uint8_t ip[IPV4_ADDR_LEN];

    static const uint8_t broadcast_ip[IPV4_ADDR_LEN];

    static const uint8_t broadcast_mac[MAC_ADDR_LEN];

    static uint8_t mac[MAC_ADDR_LEN];

    static void init();

    static void run();

    static void sendPacket(Ethernet::packet_info* packet);

    /*** Compute Internet Checksum for "count" bytes
     *         beginning at location "addr".
     * Taken from https://tools.ietf.org/html/rfc1071
     ***/
    static uint16_t checksum(const void* addr, size_t count);
};


#endif //NETWORK_H
