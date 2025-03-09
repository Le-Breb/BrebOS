#ifndef NETWORK_H
#define NETWORK_H
#include "E1000.h"
#include "NetworkConsts.h"

class Network
{
    static E1000* nic;

public:
    static uint8_t ip[IPV4_ADDR_LEN];

    static const uint8_t broadcast_ip[IPV4_ADDR_LEN];

    static const uint8_t broadcast_mac[MAC_ADDR_LEN];

    static uint8_t gateway_ip[IPV4_ADDR_LEN];

    static uint8_t mac[MAC_ADDR_LEN];

    static uint8_t null_mac[MAC_ADDR_LEN];

    static uint8_t gateway_mac[MAC_ADDR_LEN];

    static uint8_t subnet_mast[IPV4_ADDR_LEN];

    static void init();

    static void run();

    static void send_packet(const Ethernet::packet_info* packet);

    /*** Compute Internet Checksum for "count" bytes
     *         beginning at location "addr".
     * Taken from https://tools.ietf.org/html/rfc1071
     ***/
    static uint16_t checksum(const void* addr, size_t count);

    [[nodiscard]] static uint32_t generate_random_id32();

    [[nodiscard]] static uint32_t generate_random_id16();

    [[nodiscard]] static uint16_t checksum_add(uint16_t checksum1, uint16_t checksum2);
};


#endif //NETWORK_H
