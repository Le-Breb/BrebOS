#ifndef NETWORK_H
#define NETWORK_H
#include "E1000.h"

#define NETWORK_IPV4_LEN 4
#define NETWORK_MAC_LEN 6

class Network
{
    static E1000* nic;

public:
    static const uint8_t ip[NETWORK_IPV4_LEN];

    static const uint8_t broadcast_ip[NETWORK_IPV4_LEN];

    static const uint8_t broadcast_mac[NETWORK_MAC_LEN];

    static uint8_t mac[NETWORK_MAC_LEN];

    static void init();

    static void run();
};


#endif //NETWORK_H
