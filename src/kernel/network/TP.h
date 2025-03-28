#ifndef TP_H
#define TP_H

#include "NetworkConsts.h"
#include "UDP.h"

#define TP_DST_PORT 37

// Time Protocol
class TP
{
    static uint32_t time;
    static bool request_sent;

    static uint8_t server_ip[IPV4_ADDR_LEN];
public:
    static bool handle_packet(const UDP::packet_t* packet);

    static void send_request();
};


#endif //TP_H
