#include "DHCP.h"

#include "Endianness.h"
#include "Network.h"
#include "UDP.h"
#include "../core/memory.h"
#include "../core/fb.h"

uint32_t DHCP::dhcp_disc_xid = 0;

size_t DHCP::get_response_size(const packet_t* packet)
{
    if (packet->hlen != sizeof(packet_t))
        return -1;
    return sizeof(packet_t);
}

uint32_t DHCP::generate_xid()
{
    uint64_t tsc;
    asm volatile ("rdtsc" : "=A"(tsc));
    return (uint32_t)(tsc & 0xFFFFFFFF) ^ (Network::mac[0] << 16);
}

void DHCP::send_discover()
{
    auto header_size = get_disc_header_size();
    auto size = header_size + UDP::get_headers_size();
    auto buf = (uint8_t*)calloc(size, 1);
    auto buf_beg = buf;
    buf = UDP::write_headers(buf, DHCP_DISC_SRC_PORT, DHCP_DISC_DST_PORT, header_size,
                             *(uint32_t*)&Network::broadcast_ip, (uint8_t*)Network::broadcast_mac);
    uint8_t opt_buf[DHCP_DISC_OPT_SIZE];
    option_t* dhcp_type_opt = (option_t*)opt_buf;
    dhcp_type_opt->code = DHCP_OPT_MSG_TYPE;
    dhcp_type_opt->len = 1;
    dhcp_type_opt->data[0] = DHCP_DISCOVER;
    option_t* dhcp_req_list_opt = (option_t*)((uint8_t*)(dhcp_type_opt + 1) + dhcp_type_opt->len);
    dhcp_req_list_opt->code = DHCP_OPT_REQUEST_LIST;
    dhcp_req_list_opt->len = 4;
    uint8_t* list_el = dhcp_req_list_opt->data;
    *list_el++ = DHCP_OPT_SUBNET_MASK;
    *list_el++ = DHCP_OPT_ROUTER;
    *list_el++ = DHCP_OPT_DOMAIN_NAME;
    *list_el++ = DHCP_OPT_DOMAIN_NAME_SERVER;
    write_header(buf, BOOT_REQUEST, generate_xid(), DHCP_FLAG_BROADCAST, 0, 0, 0, 0, opt_buf, 2);

    auto ethernet = (Ethernet::packet_t*)buf_beg;
    auto packet_info = Ethernet::packet_info(ethernet, size);
    Network::send_packet(&packet_info);
}

void DHCP::send_request(const packet_t* offer_packet)
{
    auto header_size = get_request_header_size();
    auto size = header_size + UDP::get_headers_size();
    auto buf = (uint8_t*)calloc(size, 1);
    auto buf_beg = buf;
    buf = UDP::write_headers(buf, DHCP_DISC_SRC_PORT, DHCP_DISC_DST_PORT, header_size,
                             *(uint32_t*)&Network::broadcast_ip, (uint8_t*)Network::broadcast_mac);
    uint8_t opt_buf[DHCP_REQUEST_OPT_SIZE]{};
    auto* dhcp_type_opt = (option_t*)opt_buf;
    dhcp_type_opt->code = DHCP_OPT_MSG_TYPE;
    dhcp_type_opt->len = 1;
    dhcp_type_opt->data[0] = DHCP_REQUEST;
    auto dhcp_req_ip_opt = (option_t*)((uint8_t*)(dhcp_type_opt + 1) + dhcp_type_opt->len);
    dhcp_req_ip_opt->code = DHCP_OPT_IP_REQUEST;
    dhcp_req_ip_opt->len = 4;
    memcpy(dhcp_req_ip_opt->data, &offer_packet->yiaddr, IPV4_ADDR_LEN);
    auto dhcp_server_id_opt = (option_t*)((uint8_t*)(dhcp_req_ip_opt + 1) + dhcp_req_ip_opt->len);
    dhcp_server_id_opt->code = DHCP_OPT_SERVER_IDENTIFIER;
    dhcp_server_id_opt->len = 4;
    memcpy(dhcp_server_id_opt->data, &offer_packet->siaddr, IPV4_ADDR_LEN);
    write_header(buf, BOOT_REQUEST, offer_packet->xid, DHCP_FLAG_BROADCAST, 0, 0, offer_packet->siaddr, 0, opt_buf, 3);

    auto ethernet = (Ethernet::packet_t*)buf_beg;
    auto packet_info = Ethernet::packet_info(ethernet, size);
    Network::send_packet(&packet_info);
}

void DHCP::write_header(uint8_t* buf, uint8_t op, uint32_t xid, uint16_t flags, uint32_t ciaddr, uint32_t yiaddr,
                        uint32_t siaddr, uint32_t giaddr, uint8_t* options_buf, size_t num_options)
{
    auto packet = (packet_t*)buf;
    packet->op = op;
    packet->htype = DHCP_HTYPE_ETHERNET;
    packet->hlen = MAC_ADDR_LEN;
    packet->hops = 0;
    printf("DHCP Discover xid: %x\n", xid);
    packet->xid = xid;
    packet->secs = 0;
    packet->flags = flags;
    packet->ciaddr = ciaddr;
    packet->yiaddr = yiaddr;
    packet->siaddr = siaddr;
    packet->giaddr = giaddr;
    memcpy(packet->chaddr, Network::mac, MAC_ADDR_LEN);
    packet->magic_cookie = Endianness::switch32(DHCP_MAGIC_COOKIE);

    size_t opt_tot_len = 0;
    bool last_isDHCP_OPT_REQUEST_LIST = false;
    for (size_t i = 0; i < num_options; i++)
    {
        auto dest_opt = (option_t*)(packet->options + opt_tot_len);
        auto src_opt = (option_t*)(options_buf + opt_tot_len);
        size_t opt_len = src_opt->len + sizeof(option_t);
        memcpy(dest_opt, src_opt, opt_len);
            opt_tot_len += opt_len;
        last_isDHCP_OPT_REQUEST_LIST = src_opt->code == DHCP_OPT_REQUEST_LIST;
    }
    if (num_options && last_isDHCP_OPT_REQUEST_LIST)
        *(packet->options + opt_tot_len) = DHCP_OPT_END;
}

size_t DHCP::get_header_size()
{
    return sizeof(packet_t);
}

bool DHCP::handle_packet(const UDP::packet_t* packet)
{
    switch (is_dhcp(packet))
    {
        case DHCP_OFFER:
            send_request((packet_t*)packet->data);
            break;
        default:
            return false;
    }

    return true;
}

uint DHCP::is_dhcp(const UDP::packet_t* packet)
{
    uint16_t src_port = Endianness::switch16(packet->header.src_port);
    uint16_t dst_port = Endianness::switch16(packet->header.dst_port);
    if (src_port != DHCP_DISC_DST_PORT || dst_port != DHCP_DISC_SRC_PORT)
        return false;
    size_t len = packet->header.length;
    if (len < sizeof(packet_t))
        return false;
    return is_dhcp_offer(packet) ? DHCP_OFFER : 0;
}

bool DHCP::is_dhcp_offer(const UDP::packet_t* packet)
{
    auto dhcp_packet = (packet_t*)packet->data;
    if (dhcp_packet->op != BOOT_REPLY)
        return false;
    if (dhcp_packet->magic_cookie != Endianness::switch32(DHCP_MAGIC_COOKIE))
        return false;
    option_t* opt = (option_t*)dhcp_packet->options;
    if ((uint8_t*)opt - packet->data + sizeof(option_t) > packet->header.length)
        return false;
    while (opt->code != DHCP_OPT_MSG_TYPE)
    {
        opt = (option_t*)((uint8_t*)opt + opt->len + sizeof(option_t));
        if ((uint8_t*)opt - packet->data + sizeof(option_t) > packet->header.length)
            return false;
    }

    return *opt->data == DHCP_OFFER;
}

size_t DHCP::get_disc_header_size()
{
    return get_header_size() + DHCP_DISC_OPT_SIZE;
}

size_t DHCP::get_request_header_size()
{
    return get_header_size() + DHCP_REQUEST_OPT_SIZE;
}
