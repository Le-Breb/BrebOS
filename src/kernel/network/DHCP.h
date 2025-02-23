#ifndef DHCP_H
#define DHCP_H

#include <kstdint.h>
#include <kstddef.h>

#define DHCP_MAGIC_COOKIE 0x63825363

#define DHCP_DISCOVER 1

#define DHCP_DISC_SRC_PORT 68
#define DHCP_DISC_DST_PORT 67

#define DHCP_HTYPE_ETHERNET   1  // Ethernet (10Mb)
#define DHCP_HTYPE_IEEE802    6  // IEEE 802 Networks (e.g., Wi-Fi)
#define DHCP_HTYPE_FRAMERELAY 15 // Frame Relay

#define DHCP_FLAG_BROADCAST 0x0080 // Broadcast flag. Bit order change because of endianness

class DHCP
{
public:
    struct packet
    {
        uint8_t op; // Message type
        uint8_t htype; // Hardware type
        uint8_t hlen; // Hardware address length
        uint8_t hops; // Number of relay agent hops

        uint32_t xid; // Transaction ID (randomly chosen by client, echoed by server)
        uint16_t secs; // Seconds elapsed since client started DHCP process
        uint16_t flags; // Flags (bit 15 = broadcast flag, other bits are reserved)

        uint32_t ciaddr; // Client IP address (if the client already has one)
        uint32_t yiaddr; // 'Your' IP address (offered by the server)
        uint32_t siaddr; // Server IP address (for the next server in boot process)
        uint32_t giaddr; // Gateway/relay agent IP address

        uint8_t chaddr[16]; // Client hardware address (e.g., MAC address)
        uint8_t sname[64]; // Optional server name (not commonly used)
        uint8_t file[128]; // Boot filename (used in network boot scenarios)

        uint32_t magic_cookie; // DHCP Magic Cookie (0x63825363), identifies DHCP options

        uint8_t options[]; // Variable-length field for DHCP options (e.g., message type, lease time)
    };


    typedef struct packet packet_t;

    static size_t get_response_size(const packet_t* packet);;

    static void handlePacket(packet_t* packet, uint8_t* response_buf);

    static void send_discover();

    static void write_header(uint8_t* buf, uint8_t op, uint16_t flags, uint32_t ciaddr, uint32_t yiaddr,
                             uint32_t siaddr,
                             uint32_t giaddr);

    [[nodiscard]] static size_t get_header_size();
};


#endif //DHCP_H
