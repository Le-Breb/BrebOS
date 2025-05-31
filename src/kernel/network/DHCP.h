#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>
#include <kstddef.h>

#include "UDP.h"

#define DHCP_MAGIC_COOKIE 0x63825363

#define BOOT_REQUEST 1
#define BOOT_REPLY 2

#define DHCP_DISCOVER  1  // Client broadcast to locate available servers
#define DHCP_OFFER     2  // Server response offering an IP address
#define DHCP_REQUEST   3  // Client request for offered IP address
#define DHCP_DECLINE   4  // Client rejects offered IP (e.g., IP conflict)
#define DHCP_ACK       5  // Server confirms client's lease
#define DHCP_NAK       6  // Server rejects request (e.g., lease expired)
#define DHCP_RELEASE   7  // Client releases its IP back to the server
#define DHCP_INFORM    8  // Client requests configuration without IP lease

#define DHCP_SRC_PORT 68
#define DHCP_DST_PORT 67

#define DHCP_HTYPE_ETHERNET   1  // Ethernet (10Mb)
#define DHCP_HTYPE_IEEE802    6  // IEEE 802 Networks (e.g., Wi-Fi)
#define DHCP_HTYPE_FRAMERELAY 15 // Frame Relay

#define DHCP_OPT_MSG_TYPE 53
#define DHCP_OPT_IP_REQUEST 50
#define DHCP_OPT_REQUEST_LIST 55
#define DHCP_OPT_REQUEST_LIST_EOL 0xFF

#pragma region DHCP_OPT_VALUES
#define DHCP_OPT_PAD 0
#define DHCP_OPT_SUBNET_MASK 1
#define DHCP_OPT_TIME_OFFSET 2
#define DHCP_OPT_ROUTER 3
#define DHCP_OPT_TIME_SERVER 4
#define DHCP_OPT_NAME_SERVER 5
#define DHCP_OPT_DOMAIN_NAME_SERVER 6
#define DHCP_OPT_LOG_SERVER 7
#define DHCP_OPT_COOKIE_SERVER 8
#define DHCP_OPT_LPR_SERVER 9
#define DHCP_OPT_IMPRESS_SERVER 10
#define DHCP_OPT_RESOURCE_LOCATION_SERVER 11
#define DHCP_OPT_HOST_NAME 12
#define DHCP_OPT_BOOT_FILE_SIZE 13
#define DHCP_OPT_MERIT_DUMP_FILE 14
#define DHCP_OPT_DOMAIN_NAME 15
#define DHCP_OPT_SWAP_SERVER 16
#define DHCP_OPT_ROOT_PATH 17
#define DHCP_OPT_EXTENSIONS_PATH 18
#define DHCP_OPT_IP_FORWARDING 19
#define DHCP_OPT_NON_LOCAL_SOURCE_ROUTING 20
#define DHCP_OPT_POLICY_FILTER 21
#define DHCP_OPT_MAX_DATAGRAM_REASSEMBLY 22
#define DHCP_OPT_DEFAULT_TTL 23
#define DHCP_OPT_PATH_MTU_AGING_TIMEOUT 24
#define DHCP_OPT_PATH_MTU_PLATEAU_TABLE 25
#define DHCP_OPT_INTERFACE_MTU 26
#define DHCP_OPT_ALL_SUBNETS_ARE_LOCAL 27
#define DHCP_OPT_BROADCAST_ADDRESS 28
#define DHCP_OPT_PERFORM_MASK_DISCOVERY 29
#define DHCP_OPT_MASK_SUPPLIER 30
#define DHCP_OPT_PERFORM_ROUTER_DISCOVERY 31
#define DHCP_OPT_ROUTER_SOLICITATION_ADDRESS 32
#define DHCP_OPT_STATIC_ROUTE 33
#define DHCP_OPT_TRAILER_ENCAPSULATION 34
#define DHCP_OPT_ARP_CACHE_TIMEOUT 35
#define DHCP_OPT_ETHERNET_ENCAPSULATION 36
#define DHCP_OPT_DEFAULT_TCP_TTL 37
#define DHCP_OPT_TCP_KEEPALIVE_INTERVAL 38
#define DHCP_OPT_TCP_KEEPALIVE_GARBAGE 39
#define DHCP_OPT_NIS_DOMAIN 40
#define DHCP_OPT_NIS_SERVERS 41
#define DHCP_OPT_NTP_SERVERS 42
#define DHCP_OPT_VENDOR_SPECIFIC_INFO 43
#define DHCP_OPT_NETBIOS_NAME_SERVER 44
#define DHCP_OPT_NETBIOS_DATAGRAM_DIST_SERVER 45
#define DHCP_OPT_NETBIOS_NODE_TYPE 46
#define DHCP_OPT_NETBIOS_SCOPE 47
#define DHCP_OPT_X_FONT_SERVER 48
#define DHCP_OPT_X_DISPLAY_MANAGER 49
#define DHCP_OPT_REQUESTED_IP_ADDRESS 50
#define DHCP_OPT_IP_ADDRESS_LEASE_TIME 51
#define DHCP_OPT_OPTION_OVERLOAD 52
#define DHCP_OPT_TFTP_SERVER_NAME 66
#define DHCP_OPT_BOOT_FILE_NAME 67
#define DHCP_OPT_DHCP_MESSAGE_TYPE 53
#define DHCP_OPT_SERVER_IDENTIFIER 54
#define DHCP_OPT_PARAMETER_REQUEST_LIST 55
#define DHCP_OPT_MESSAGE 56
#define DHCP_OPT_MAX_DHCP_MESSAGE_SIZE 57
#define DHCP_OPT_RENEWAL_TIME_VALUE 58
#define DHCP_OPT_REBINDING_TIME_VALUE 59
#define DHCP_OPT_VENDOR_CLASS_IDENTIFIER 60
#define DHCP_OPT_CLIENT_IDENTIFIER 61
#define DHCP_OPT_NETWARE_IP_DOMAIN_NAME 62
#define DHCP_OPT_NETWARE_IP_SUBOPTIONS 63
#define DHCP_OPT_NISPLUS_DOMAIN 64
#define DHCP_OPT_NISPLUS_SERVERS 65
#define DHCP_OPT_MOBILE_IP_HOME_AGENT 68
#define DHCP_OPT_SMTP_SERVER 69
#define DHCP_OPT_POP3_SERVER 70
#define DHCP_OPT_NNTP_SERVER 71
#define DHCP_OPT_WWW_SERVER 72
#define DHCP_OPT_FINGER_SERVER 73
#define DHCP_OPT_IRC_SERVER 74
#define DHCP_OPT_STREETTALK_SERVER 75
#define DHCP_OPT_STREETTALK_DIRECTORY_ASSISTANCE_SERVER 76
#define DHCP_OPT_USER_CLASS 77
#define DHCP_OPT_SLP_DIRECTORY_AGENT 78
#define DHCP_OPT_SLP_SERVICE_SCOPE 79
#define DHCP_OPT_RAPID_COMMIT 80
#define DHCP_OPT_CLIENT_FQDN 81
#define DHCP_OPT_RELAY_AGENT_INFORMATION 82
#define DHCP_OPT_ISNS 83
#define DHCP_OPT_NDS_SERVERS 85
#define DHCP_OPT_NDS_TREE_NAME 86
#define DHCP_OPT_NDS_CONTEXT 87
#define DHCP_OPT_BCMCS_CONTROLLER_DOMAIN_NAME_LIST 88
#define DHCP_OPT_BCMCS_CONTROLLER_IPV4_ADDRESS_LIST 89
#define DHCP_OPT_AUTHENTICATION 90
#define DHCP_OPT_CLIENT_LAST_TRANSACTION_TIME 91
#define DHCP_OPT_ASSOCIATED_IP_ADDRESS 92
#define DHCP_OPT_CLIENT_SYSTEM 93
#define DHCP_OPT_CLIENT_NDI 94
#define DHCP_OPT_LDAP 95
#define DHCP_OPT_UUID_GUID 97
#define DHCP_OPT_USER_AUTHENTICATION_PROTOCOL 98
#define DHCP_OPT_GEOCONF_CIVIC 99
#define DHCP_OPT_PCODE 100
#define DHCP_OPT_TCODE 101
#define DHCP_OPT_NETINFO_PARENT_SERVER_ADDRESS 112
#define DHCP_OPT_NETINFO_PARENT_SERVER_TAG 113
#define DHCP_OPT_URL 114
#define DHCP_OPT_AUTO_CONFIGURE 116
#define DHCP_OPT_NAME_SERVICE_SEARCH 117
#define DHCP_OPT_SUBNET_SELECTION 118
#define DHCP_OPT_DOMAIN_SEARCH 119
#define DHCP_OPT_SIP_SERVERS 120
#define DHCP_OPT_CLASSLESS_STATIC_ROUTE 121
#define DHCP_OPT_CCC 122
#define DHCP_OPT_GEOCONF 123
#define DHCP_OPT_VI_VENDOR_CLASS 124
#define DHCP_OPT_VI_VENDOR_INFO 125
#define DHCP_OPT_TFTP_SERVER_IP 150
#define DHCP_OPT_MUD_URL 161
#define DHCP_OPT_EXPERIMENTAL_PRIVATE 224
#define DHCP_OPT_END 255
#pragma endregion

#define DHCP_DISC_OPT_SIZE sizeof(option_t) + 1 + sizeof(option_t) + 4 + 1
#define DHCP_REQUEST_OPT_SIZE sizeof(option_t) + 1 + sizeof(option_t) + 4 + sizeof(option_t) + 4 + 1

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

    struct option
    {
        uint8_t code;
        uint8_t len;
        uint8_t data[];
    };

    typedef struct option option_t;


    typedef struct packet packet_t;

    static void send_discover();

    static void send_request(const packet_t* offer_packet);

    static void write_header(uint8_t* buf, uint8_t op, uint32_t xid, uint16_t flags, uint32_t ciaddr,
                             uint32_t yiaddr, uint32_t siaddr, uint32_t giaddr, uint8_t* options_buf,
                             size_t num_options);

    [[nodiscard]] static size_t get_header_size();

    static bool handle_packet(const UDP::packet_t* udp_packet);

private:
    [[nodiscard]] static size_t get_disc_header_size();

    [[nodiscard]] static size_t get_request_header_size();

    [[nodiscard]] static bool is_dhcp_offer(const UDP::packet_t* packet);

    [[nodiscard]] static bool is_dhcp_ack(const UDP::packet_t* packet);

    /**
     * Checks if a packet is a valid DHCP packet
     * @param packet packet to be checked
     * @return 0 if packet is invalid, packet type otherwise
     */
    [[nodiscard]] static uint packet_valid(const UDP::packet_t* packet);

    static void handle_offer(const packet_t* packet);

    static void handle_ack(const packet_t* packet);

    static uint32_t disc_id;

    static uint8_t server_mac[MAC_ADDR_LEN];
};


#endif //DHCP_H
