#ifndef DNS_H
#define DNS_H
#include <kstdint.h>
#include <kstddef.h>

#include "NetworkConsts.h"
#include "Socket.h"
#include "UDP.h"

#define DNS_PORT 53

#define DNS_QUERY 0 << 15
#define DNS_REPLY 1 << 15

#define DNS_RECURSION_DESIRED 1 << 8

#define DNS_OPCODE_QUERY 0 << 11
#define DNS_OPCODE_IQUERY 1 << 11
#define DNS_OPCODE_STATUS 2 << 11

#define DNS_REQUEST_A      1   // A record (IPv4 address)
#define DNS_REQUEST_NS     2   // NS record (Name server)
#define DNS_REQUEST_CNAME  5   // CNAME record (Canonical name)
#define DNS_REQUEST_SOA    6   // SOA record (Start of authority)
#define DNS_REQUEST_PTR    12  // PTR record (Pointer)
#define DNS_REQUEST_MX     15  // MX record (Mail exchange)
#define DNS_REQUEST_TXT    16  // TXT record (Text)
#define DNS_REQUEST_AAAA   28  // AAAA record (IPv6 address)
#define DNS_REQUEST_SRV    33  // SRV record (Service locator)
#define DNS_REQUEST_ANY    255 // ANY record (All records)

#define DNS_IN 1 // Internet class
#define DNS_CS 2 // CSNET class
#define DNS_CH 3 // CHAOS class

#define DNS_PENDING_QUEUE_SIZE 10

class DNS
{
public:
    struct header
    {
        uint16_t id;
        uint16_t flags;
        uint16_t quest_count;
        uint16_t ans_count;
        uint16_t auth_count;
        uint16_t add_count;
    } __attribute__((packed));

    typedef struct header header_t;

    struct question_content
    {
        uint16_t qtype;
        uint16_t qclass;
    } __attribute__((packed));

    typedef struct question_content question_content_t;

    struct question
    {
        const char* name;
        question_content* content;
    };

    typedef struct question question_t;

    struct resource_record_content
    {
        uint16_t type;
        uint16_t class_;
        uint32_t ttl;
        uint16_t data_len;
        uint8_t data[];
    } __attribute__((packed));

    typedef struct resource_record_content resource_record_content_t;

    struct resource_record
    {
        const char* name;
        resource_record_content* content;
    };

    typedef struct resource_record resource_record_t;

private:
    static uint16_t last_query_id;

    struct pending_queue_entry
    {
        Socket* socket = nullptr;
        const char* hostname = nullptr;
    };

    // Queue of packets sockets waiting for their destination IP to be resolved
    static pending_queue_entry pending_queue[DNS_PENDING_QUEUE_SIZE];
    static size_t pending_queue_size;

    [[nodiscard]] static size_t get_header_size();

    [[nodiscard]] static size_t get_question_size(const char* resource_name);

    [[nodiscard]] static size_t get_resource_record_size(const resource_record_t* record);

    [[nodiscard]] static size_t get_encoded_name_len(const char* name);

    /**
     * Writes name in DNS fashion
     * @param buf where to write the encoded name
     * @param name name to encode
     * @return pointer pointing right after the end of the written encoded name
     */
    static uint8_t* write_encoded_name(uint8_t* buf, const char* name);

    static void write_header(uint8_t* buf, const question_t& question);

    /**
     * Parses a DNS encoded name
     * @param encoded_name name to parse
     * @param packet_beg beginning of the packet that encoded name belongs to
     * @return string containing decoded name
     */
    [[nodiscard]] static char* parse_name(const char* encoded_name, const uint8_t* packet_beg);

    static void display_response(const header_t* header);

    static const uint8_t google_dns_ip[IPV4_ADDR_LEN];

public:
    static void resolve_hostname(const char* hostname, Socket* socket);

    static bool handle_packet(const UDP::packet_t* packet);
};


#endif //DNS_H
