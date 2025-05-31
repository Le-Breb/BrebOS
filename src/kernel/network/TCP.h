#ifndef TCP_H
#define TCP_H

#include <stdint.h>
#include <kstddef.h>

#include "IPV4.h"
#include "NetworkConsts.h"

#define DEFAULT_TCP_WINDOW_SIZE 0xFFFF

#define TCP_FLAG_FIN  0x01  // Finish: No more data from sender
#define TCP_FLAG_SYN  0x02  // Synchronize sequence numbers
#define TCP_FLAG_RST  0x04  // Reset the connection
#define TCP_FLAG_PSH  0x08  // Push function (promptly deliver to app)
#define TCP_FLAG_ACK  0x10  // Acknowledgment field significant
#define TCP_FLAG_URG  0x20  // Urgent pointer field significant
#define TCP_FLAG_ECE  0x40  // ECN-Echo (RFC 3168, congestion control)
#define TCP_FLAG_CWR  0x80  // Congestion Window Reduced (RFC 3168)
#define TCP_FLAG_NS   0x100 // ECN Nonce Concealment Protection (RFC 3540)

class Socket;

class TCP
{
    friend class Socket;

public:
    enum class State
    {
        LISTEN,
        SYN_SENT,
        SYN_RECEIVED,
        ESTABLISHED,
        FIN_WAIT_1,
        FIN_WAIT_2,
        CLOSE_WAIT,
        CLOSING,
        LAST_ACK,
        TIME_WAIT,
        CLOSED
    };

    struct pseudo_header
    {
        uint32_t src_ip;
        uint32_t dest_ip;
        uint8_t reserved;
        uint8_t protocol;
        uint16_t tcp_len;
    } __attribute__((packed));

    typedef struct pseudo_header pseudo_header_t;

    struct header
    {
        uint16_t src_port;
        uint16_t dest_port;
        uint32_t seq_num;
        uint32_t ack_num;
        uint8_t header_len;
        uint8_t flags;
        uint16_t window_size;
        uint16_t checksum;
        uint16_t urgent_pointer;
        uint32_t options[];
    } __attribute__((packed));

    typedef struct header header_t;

    struct packet_info
    {
        header_t* packet;
        size_t size;
    };

    typedef struct packet_info packet_info_t;

    [[nodiscard]] static size_t get_header_size();

private:

    static void write_header(uint8_t* buf, const Socket* socket, uint8_t flags, uint16_t data_size);

    static void write_sync_header(uint8_t* buf, const Socket* socket);

    static void write_ack_header(uint8_t* buf, const Socket* socket, bool fin = false);

    static void write_reset_header(uint8_t* buf, const Socket* socket);

    static void write_fin_ack_header(uint8_t* buf, const Socket* socket);

    static void write_push_ack_header(uint8_t* buf, const Socket* socket, uint16_t data_size);

    static uint16_t compute_checksum(const header_t* header, uint32_t dest_ip, uint16_t payload_size);

    static void send_fin_ack(Socket* socket);

    static void fill_pseudo_header(pseudo_header_t& pseudo_header, const header_t* header, uint32_t dest_ip, uint16_t payload_size);

    static void send_sync(const Socket* socket);

    [[nodiscard]] static bool packet_valid(const packet_info_t* packet_info);
public:
    static uint16_t handle_packet(const packet_info_t* packet_info, const IPV4::packet_t* ipv4_packet, const Ethernet::packet_t* ethernet_packet);

    static void send_data(const void* data, uint16_t data_size, const Socket* socket);
};

#endif //TCP_H
