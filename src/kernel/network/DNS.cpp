#include "DNS.h"

#include "ARP.h"
#include "Endianness.h"
#include "Network.h"
#include "UDP.h"
#include "../core/fb.h"
#include "../core/memory.h"

const uint8_t DNS::google_dns_ip[IPV4_ADDR_LEN] = {8, 8, 8, 8};
uint16_t DNS::last_query_id = 0;

size_t DNS::get_header_size()
{
    return sizeof(header_t);
}

size_t DNS::get_question_size(const char* resource_name)
{
    return get_header_size() + get_encoded_name_len(resource_name) + sizeof(question_content);
}

size_t DNS::get_encoded_name_len(const char* name)
{
    size_t len = strlen(name);
    size_t num_dots = 0;
    while (*name != 0)
    {
        if (*name++ == '.')
            num_dots++;
    }

    return len + 2; // +1 for the null terminator, +1 for first len
}

uint8_t* DNS::write_encoded_name(uint8_t* buf, const char* name)
{
    uint8_t len = 0;
    while (*name != 0)
    {
        if (*name == '.')
        {
            *buf++ = len;
            memcpy(buf, name - len, len);
            buf += len;
            len = 0;
        }
        else
            len++;
        name++;
    }
    *buf++ = len;
    memcpy(buf, name - len, len);
    buf += len;
    *buf++ = '\0';

    return buf;
}

void DNS::write_header(uint8_t* buf, const question_t& question)
{
    auto header = (header_t*)buf;
    header->id = last_query_id = Network::generate_random_id16();
    header->flags = Endianness::switch16(DNS_QUERY | DNS_OPCODE_QUERY | DNS_RECURSION_DESIRED);
    header->quest_count = Endianness::switch16(1);
    buf += sizeof(header_t);
    buf = write_encoded_name(buf, question.name);
    memcpy(buf, question.content, sizeof(question_content));
}

char* parse_name_aux(const char* name, const uint8_t* packet_beg, bool& is_end)
{
    // Check if it's a pointer
    if ((*name & 0xC0) == 0xC0)
    {
        const auto offset = Endianness::switch16(*(uint16_t*)name) & 0x3FFF;
        return parse_name_aux((const char*)packet_beg + offset, packet_beg, is_end);
    }

    size_t buf_capacity = 5;
    size_t buf_size = 0;
    auto res = new char[buf_capacity];

    size_t idx = 0;
    size_t res_idx = 0;

    while (name[idx] != '\0' && !is_end)
    {
        unsigned char l = name[idx];
        bool pointer = (l & 0xC0) == 0xC0;
        const char* sub = name + idx + 1;

        // Recurse if it's a pointer
        if (pointer)
        {
            const auto offset = Endianness::switch16(*(uint16_t*)(name + idx)) & 0x3FFF;
            sub = parse_name_aux((const char*)packet_beg + offset, packet_beg, is_end);
            l = strlen(sub);
        }

        // Resize buffer if needed
        if (buf_size + l + 1 > buf_capacity)
        {
            buf_capacity *= 2;
            auto nb = new char[buf_capacity];
            memcpy(nb, res, buf_size);
            delete res;
            res = nb;
        }

        // Copy the subdomain
        memcpy(res + res_idx, sub, l);
        *(res + idx + l) = '.';
        buf_size += l + 1;

        // Update indexes
        idx += pointer ? 2 : l + 1;
        res_idx += l + 1;

        if (pointer)
            delete sub;
    }

    res[res_idx - 1] = '\0';
    is_end = true;
    // Indicate to caller that the name is fully parsed (a pointer can be the last element, thus not null-terminated)

    return res;
}

char* DNS::parse_name(const char* encoded_name, const uint8_t* packet_beg)
{
    bool is_end = false;
    return parse_name_aux(encoded_name, packet_beg, is_end);
}

void DNS::display_response(const header_t* header)
{
    auto b = (uint8_t*)(header + 1);

    // Skip queries
    for (size_t i = 0; i < Endianness::switch16(header->quest_count); i++)
    {
        bool pointer = (*b & 0xC0) == 0xC0;
        auto name_len = pointer ? 2 : strlen((const char*)b) + 1;
        //[[maybe_unused]] auto query = (question_content_t*)(b + name_len);
        auto rc_len = name_len + sizeof(question_content_t);
        b += rc_len;
    }

    // Display answers
    for (size_t i = 0; i < Endianness::switch16(header->ans_count); i++)
    {
        bool pointer = (*b & 0xC0) == 0xC0;
        auto name_len = pointer ? 2 : strlen((const char*)b) + 1;
        auto* rc = (resource_record_content_t*)(b + name_len);

        // Skip non IN class
        if (Endianness::switch16(rc->class_) != DNS_IN)
        {
            b = (uint8_t*)(rc + 1) + Endianness::switch16(rc->data_len);
            continue;
        }

        const char* name = parse_name((const char*)b, (uint8_t*)header);
        switch (Endianness::switch16(rc->type))
        {
            case DNS_REQUEST_CNAME:
            {
                const char* alias_name = parse_name((const char*)rc->data, (uint8_t*)header);
                printf("%s is an alias for %s\n", name, alias_name);
                delete alias_name;
                break;
            }
            case DNS_REQUEST_A:
                printf("%s has address %d.%d.%d.%d\n", name, rc->data[0], rc->data[1], rc->data[2], rc->data[3]);
                break;
            default:
                break;
        }
        delete name;

        b = (uint8_t*)(rc + 1) + Endianness::switch16(rc->data_len);
    }
}

void DNS::send_query(const char* hostname)
{
    size_t dns_packet_size = get_question_size(hostname);
    Ethernet::packet_info_t response_info;
    uint8_t dest_mac[MAC_ADDR_LEN];
    ARP::get_mac(google_dns_ip, dest_mac);
    auto* buf = UDP::create_packet(DNS_PORT, DNS_PORT, dns_packet_size, *(uint32_t*)&google_dns_ip, dest_mac,
                                   response_info);

    question_content_t question_content{Endianness::switch16(DNS_REQUEST_A), Endianness::switch16(DNS_IN)};
    question_t question{hostname, &question_content};
    write_header(buf, question);

    Network::send_packet(&response_info);
}

bool DNS::handle_packet(const UDP::packet_t* packet)
{
    if (Endianness::switch16(packet->header.dst_port) != DNS_PORT ||
        Endianness::switch16(packet->header.length) < get_header_size())
        return false;

    auto header = (header_t*)packet->payload;
    if (header->id != last_query_id)
        return false;

    display_response((header_t*)packet->payload);

    return true;
}
