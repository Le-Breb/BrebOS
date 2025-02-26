#include "DHCP.h"

#include "Endianness.h"
#include "Network.h"
#include "UDP.h"
#include "../core/memory.h"
#include "../core/fb.h"

size_t DHCP::get_response_size(const packet_t* packet)
{
    if (packet->hlen != sizeof(packet_t))
        return -1;
    return sizeof(packet_t);
}

uint32_t get_xid()
{
    uint64_t tsc;
    asm volatile ("rdtsc" : "=A"(tsc));
    return (uint32_t)(tsc & 0xFFFFFFFF) ^ (Network::mac[0] << 16);
}

void DHCP::send_discover()
{
    auto size = get_disc_header_size() + UDP::get_headers_size();
    auto buf = (uint8_t*)calloc(size, 1);
    auto buf_beg = buf;
    buf = UDP::write_headers(buf, DHCP_DISC_SRC_PORT, DHCP_DISC_DST_PORT, get_disc_header_size(),
                             *(uint32_t*)&Network::broadcast_ip, (uint8_t*)Network::broadcast_mac);
    uint8_t opt_buf[DHCP_DISC_OPT_SIZE];
    option_t* dhcp_disc_opt = (option_t*)opt_buf;
    dhcp_disc_opt->code = DHCP_OPT_MSG_TYPE;
    dhcp_disc_opt->len = 1;
    dhcp_disc_opt->data[0] = DHCP_DISCOVER;
    option_t* dhcp_req_list_opt = (option_t*)((uint8_t*)(dhcp_disc_opt + 1) + dhcp_disc_opt->len);
    dhcp_req_list_opt->code = DHCP_OPT_REQUEST_LIST;
    dhcp_req_list_opt->len = 4;
    uint8_t* list_el = dhcp_req_list_opt->data;
    *list_el++ = DHCP_OPT_SUBNET_MASK;
    *list_el++ = DHCP_OPT_ROUTER;
    *list_el++ = DHCP_OPT_DOMAIN_NAME;
    *list_el++ = DHCP_OPT_DOMAIN_NAME_SERVER;
    write_header(buf, DHCP_DISCOVER, DHCP_FLAG_BROADCAST, 0, 0, 0, 0, opt_buf, 2);

    auto ethernet = (Ethernet::packet_t*)buf_beg;
    auto packet_info = Ethernet::packet_info(ethernet, size);
    Network::send_packet(&packet_info);
}

void DHCP::write_header(uint8_t* buf, uint8_t op, uint16_t flags, uint32_t ciaddr, uint32_t yiaddr, uint32_t siaddr,
                        uint32_t giaddr, uint8_t* options_buf, size_t num_options)
{
    auto packet = (packet_t*)buf;
    packet->op = op;
    packet->htype = DHCP_HTYPE_ETHERNET;
    packet->hlen = MAC_ADDR_LEN;
    packet->hops = 0;
    auto xid = get_xid();
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
    for (size_t i = 0; i < num_options; i++)
    {
        auto dest_opt = (option_t*)(packet->options + opt_tot_len);
        auto src_opt = (option_t*)(options_buf + opt_tot_len);
        size_t opt_len = src_opt->len + sizeof(option_t);
        memcpy(dest_opt, src_opt, opt_len);
        opt_tot_len += opt_len;
    }
    if (num_options)
        *(packet->options + opt_tot_len) = DHCP_OPT_END;
}

size_t DHCP::get_header_size()
{
    return sizeof(packet_t);
}

size_t DHCP::get_disc_header_size()
{
    return get_header_size() + DHCP_DISC_OPT_SIZE;
}
