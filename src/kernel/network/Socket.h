#ifndef SOCKET_H
#define SOCKET_H
#include <kstdint.h>

#include "Network.h"
#include "TCPListener.h"
#include "TCP.h"
#include "../utils/list.h"

#define WAITING_QUEUE_CAPACITY 10

class Socket
{
    public:
    struct packet
    {
        const void* data;
        uint16_t size;
    };

    typedef struct packet packet_t;
    private:
    friend class TCP;
    uint16_t port = TCP::random_ephemeral_port();
    uint16_t peer_port;
    uint8_t ip[IPV4_ADDR_LEN]{};
    uint8_t peer_ip[IPV4_ADDR_LEN]{};
    TCP::State state = TCP::State::CLOSED;
    uint32_t seq_num = Network::generate_random_id32();
    uint32_t ack_num = 0;
    TCP_listener* listener = nullptr;

    static uint16_t socket_head;
    static list<Socket*> sockets;

    uint16_t waiting_queue_size = 0;
    uint16_t waiting_queue_start = 0;
    packet_t* waiting_queue[WAITING_QUEUE_CAPACITY]{};

    void acknowledge(const TCP::header_t* packet, uint8_t* response_buf);

    void flush_waiting_queue();
public:
    Socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port);

    void set_listener(TCP_listener* listener);

    ~Socket();

    void initialize();

    void handle_packet(const TCP::header_t* packet, uint8_t* response_buf, uint16_t packet_size, const Ethernet::packet_info_t* response_info);

    [[nodiscard]] static Socket* port_used(uint16_t port);

    static void close_all_connections();

    static Socket* reset_response_socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port, uint32_t peer_seq_num);

    void send_data(const void* data, uint16_t size);
};


#endif //SOCKET_H
