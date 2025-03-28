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
        const void* data = nullptr;
        uint16_t size = 0;
    };

    typedef struct packet packet_t;

private:
    friend class TCP;
    const char* hostname;
    uint16_t port = Network::random_ephemeral_port();
    uint16_t peer_port;
    uint8_t ip[IPV4_ADDR_LEN]{};
    uint8_t peer_ip[IPV4_ADDR_LEN]{};
    TCP::State state = TCP::State::CLOSED;
    uint32_t seq_num = Network::generate_random_id32();
    uint32_t ack_num = 0;
    TCP_listener* listener = nullptr; // Listener to send callbacks to

    packet_t start_packet; // Packet to send when handhsake is completed

    static list<Socket*> sockets; // List of all sockets

    void acknowledge(const TCP::packet_info_t* packet_info, uint8_t* response_buf, bool fin = false);

    // Creates a buffer set up to respond to a received packet
    [[nodiscard]] static uint8_t* create_packet_response(uint16_t payload_size, const IPV4::packet_t* ipv4_packet,
                                                         const Ethernet::packet_t* ethernet_packet,
                                                         Ethernet::packet_info_t& response_info);
public:
    Socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port);

    Socket(const char* hostname, uint16_t peer_port);

    void set_listener(TCP_listener* listener);

    ~Socket();

    void initialize();

    void handle_packet(const TCP::packet_info_t* packet_info, const IPV4::packet_t* ipv4_packet,
                       const Ethernet::packet_t* ethernet_packet);

    [[nodiscard]] static Socket* port_used(uint16_t port);

    static void close_all_connections();

    // Creates a dummy socket in order to send RST in unhandled situations
    static Socket* reset_response_socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port, uint32_t peer_seq_num);

    void send_data(const void* data, uint16_t size);

    void close(const char* error_message = nullptr);

    [[nodiscard]] const uint8_t* get_peer_ip() const;

    [[nodiscard]] uint16_t get_peer_port() const;

    [[nodiscard]] const char* get_hostname() const;

    void on_hostname_resolved(const uint8_t peer_ip[IPV4_ADDR_LEN]);
};


#endif //SOCKET_H
