#ifndef ETHERNET_H
#define ETHERNET_H
#include <kstdint.h>
#include <kstddef.h>
#include <kstring.h>

#include "NetworkConsts.h"

#define ETHERTYPE_IPV4      0x0800  // Internet Protocol version 4 (IPv4)
#define ETHERTYPE_ARP       0x0806  // Address Resolution Protocol (ARP)
#define ETHERTYPE_IPV6      0x86DD  // Internet Protocol version 6 (IPv6)
#define ETHERTYPE_VLAN      0x8100  // IEEE 802.1Q VLAN tagging
#define ETHERTYPE_QINQ      0x88A8  // IEEE 802.1ad Q-in-Q VLAN tagging
#define ETHERTYPE_MPLS_U    0x8847  // MPLS Unicast
#define ETHERTYPE_MPLS_M    0x8848  // MPLS Multicast
#define ETHERTYPE_PPPOE_DISC 0x8863 // PPPoE Discovery Stage
#define ETHERTYPE_PPPOE_SESS 0x8864 // PPPoE Session Stage
#define ETHERTYPE_FLOW_CTRL 0x8808  // Ethernet Flow Control (Pause Frame)
#define ETHERTYPE_MACSEC    0x88E5  // MAC Security (IEEE 802.1AE - MACsec)
#define ETHERTYPE_FCOE      0x8906  // Fibre Channel over Ethernet (FCoE)
#define ETHERTYPE_ECP       0x8915  // IEEE 802.1Qbg Edge Control Protocol (ECP)


class Ethernet
{
public:
    struct header
    {
        uint8_t dest[MAC_ADDR_LEN]{};
        uint8_t src[MAC_ADDR_LEN]{};
        uint16_t type;
    };

    typedef struct header header_t;

    struct packet
    {
        header_t header;
        uint8_t payload[];
    };

    typedef struct packet packet_t;

    struct packet_info
    {
        packet_t* packet;
        uint8_t size;

        packet_info(packet_t* packet, uint8_t size)
            : packet(packet),
              size(size)
        {
        }
    };

    typedef struct packet_info packet_info_t;

    static void handle_packet(packet_info* packet_info);

    static size_t get_headers_size();

    /**
     * Compute the size of the buffer that will contain the response to a packet
     * @param packet_info Ethernet packet info
     * @return size of response. 0 if no response needed, -1 packet shouldn't be processed
     * (if error or if packet is not destined to us)
     */
    static size_t get_response_size(packet_info* packet_info);

    static uint8_t* write_header(uint8_t* buf, uint8_t dest[MAC_ADDR_LEN], uint8_t src[MAC_ADDR_LEN], uint16_t type);
};


#endif //ETHERNET_H
