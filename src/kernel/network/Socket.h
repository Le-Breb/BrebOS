#ifndef SOCKET_H
#define SOCKET_H
#include <kstdint.h>

#include "Network.h"
#include "TCP.h"
#include "../utils/list.h"

class Socket
{
    friend class TCP;
    uint16_t port = TCP::random_ephemeral_port();
    uint16_t peer_port;
    uint8_t ip[IPV4_ADDR_LEN]{};
    uint8_t peer_ip[IPV4_ADDR_LEN]{};
    TCP::State state = TCP::State::CLOSED;
    uint32_t seq_num = Network::generate_random_id32();
    uint32_t ack_num = 0;

    static uint16_t socket_head;
    static list<Socket*> sockets;

    void acknowledge(const TCP::header_t* packet, uint8_t* response_buf);
public:
    Socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port);

    ~Socket();

    void initialize();

    void handle_packet(const TCP::header_t* packet, uint8_t* response_buf);

    [[nodiscard]] static Socket* port_used(uint16_t port);

    static void close_all_connections();

    static Socket* reset_response_socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port, uint32_t peer_seq_num);
};


#endif //SOCKET_H
