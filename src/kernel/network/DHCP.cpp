#include "DHCP.h"

#include "ARP.h"
#include "DNS.h"
#include "Endianness.h"
#include "Network.h"
#include "UDP.h"
#include "../core/memory.h"
#include "../core/fb.h"

uint32_t DHCP::disc_id = 0;
uint8_t DHCP::server_mac[MAC_ADDR_LEN] = {};

void DHCP::send_discover()
{
    auto header_size = get_request_header_size();
    Ethernet::packet_info_t response_info;
    auto buf = UDP::create_packet(DHCP_SRC_PORT, DHCP_DST_PORT, header_size, *(uint32_t*)Network::broadcast_ip,
                                  (uint8_t*)Network::broadcast_mac, response_info);

    // Write options
    uint8_t opt_buf[DHCP_DISC_OPT_SIZE];
    auto dhcp_type_opt = (option_t*)opt_buf;

    // Message type
    dhcp_type_opt->code = DHCP_OPT_MSG_TYPE;
    dhcp_type_opt->len = 1;
    dhcp_type_opt->data[0] = DHCP_DISCOVER;

    // Request list
    auto dhcp_req_list_opt = (option_t*)((uint8_t*)(dhcp_type_opt + 1) + dhcp_type_opt->len);
    dhcp_req_list_opt->code = DHCP_OPT_REQUEST_LIST;
    dhcp_req_list_opt->len = 4;
    uint8_t* list_el = dhcp_req_list_opt->data;
    *list_el++ = DHCP_OPT_SUBNET_MASK;
    *list_el++ = DHCP_OPT_ROUTER;
    *list_el++ = DHCP_OPT_DOMAIN_NAME;
    *list_el++ = DHCP_OPT_DOMAIN_NAME_SERVER;

    write_header(buf, BOOT_REQUEST, Network::generate_random_id32(), DHCP_FLAG_BROADCAST, 0, 0, 0, 0, opt_buf, 2);

    Network::send_packet(&response_info);
}

void DHCP::send_request(const packet_t* offer_packet)
{
    auto header_size = get_request_header_size();
    Ethernet::packet_info_t response_info;
    auto buf = UDP::create_packet(DHCP_SRC_PORT, DHCP_DST_PORT, header_size, *(uint32_t*)Network::broadcast_ip,
                                  (uint8_t*)Network::broadcast_mac, response_info);

    // Options
    uint8_t opt_buf[DHCP_REQUEST_OPT_SIZE]{};
    auto* dhcp_type_opt = (option_t*)opt_buf;

    // Message type
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

    Network::send_packet(&response_info);
}

void DHCP::write_header(uint8_t* buf, uint8_t op, uint32_t xid, uint16_t flags, uint32_t ciaddr, uint32_t yiaddr,
                        uint32_t siaddr, uint32_t giaddr, uint8_t* options_buf, size_t num_options)
{
    auto packet = (packet_t*)buf;
    packet->op = op;
    packet->htype = DHCP_HTYPE_ETHERNET;
    packet->hlen = MAC_ADDR_LEN;
    packet->hops = 0;
    packet->xid = xid;
    packet->secs = 0;
    packet->flags = flags;
    packet->ciaddr = ciaddr;
    packet->yiaddr = yiaddr;
    packet->siaddr = siaddr;
    packet->giaddr = giaddr;
    memcpy(packet->chaddr, Network::mac, MAC_ADDR_LEN);
    packet->magic_cookie = Endianness::switch32(DHCP_MAGIC_COOKIE);

    // Write options
    size_t opt_tot_len = 0;
    for (size_t i = 0; i < num_options; i++)
    {
        auto dest_opt = (option_t*)(packet->options + opt_tot_len);
        auto src_opt = (option_t*)(options_buf + opt_tot_len);
        size_t opt_len = src_opt->len + sizeof(option_t);
        memcpy(dest_opt, src_opt, opt_len);
        opt_tot_len += opt_len;
    }
    *(packet->options + opt_tot_len) = DHCP_OPT_END; // Last option marker
}

size_t DHCP::get_header_size()
{
    return sizeof(packet_t);
}

bool DHCP::handle_packet(const UDP::packet_t* udp_packet)
{
    switch (packet_valid(udp_packet))
    {
        case DHCP_OFFER:
            handle_offer((packet_t*)udp_packet->payload);
            break;
        case DHCP_ACK:
            handle_ack((packet_t*)udp_packet->payload);
            break;
        default:
            return false;
    }

    return true;
}

uint DHCP::packet_valid(const UDP::packet_t* packet)
{
    uint16_t src_port = Endianness::switch16(packet->header.src_port);
    uint16_t dst_port = Endianness::switch16(packet->header.dst_port);
    if (src_port != DHCP_DST_PORT || dst_port != DHCP_SRC_PORT)
        return false;
    size_t len = packet->header.length;
    if (len < sizeof(packet_t))
        return false;
    auto dhcp_packet = (packet_t*)packet->payload;
    if (dhcp_packet->op != BOOT_REPLY)
        return false;
    if (dhcp_packet->magic_cookie != Endianness::switch32(DHCP_MAGIC_COOKIE))
        return false;
    return is_dhcp_offer(packet) ? DHCP_OFFER : (is_dhcp_ack(packet) ? DHCP_ACK : 0);
}

void DHCP::handle_offer(const packet_t* packet)
{
    memcpy(server_mac, packet->chaddr, MAC_ADDR_LEN);
    send_request(packet);
}

void DHCP::handle_ack(const packet_t* packet)
{
    memcpy(Network::ip, &packet->yiaddr, IPV4_ADDR_LEN);

    auto* opt = (option_t*)packet->options;
    while (opt->code != DHCP_OPT_END)
    {
        switch (opt->code)
        {
            case DHCP_OPT_ROUTER:
                memcpy(Network::gateway_ip, opt->data, IPV4_ADDR_LEN);
                break;
            case DHCP_OPT_SUBNET_MASK:
                memcpy(Network::subnet_mast, opt->data, IPV4_ADDR_LEN);
                break;
            default:
                break;
        }
        opt = (option_t*)((uint8_t*)opt + opt->len + sizeof(option_t));
    }


    printf("Received IP: %d.%d.%d.%d\n", Network::ip[0], Network::ip[1], Network::ip[2], Network::ip[3]);
    printf("Gateway is: %d.%d.%d.%d\n", Network::gateway_ip[0], Network::gateway_ip[1], Network::gateway_ip[2],
           Network::gateway_ip[3]);
    ARP::resolve_gateway_mac();
}

bool DHCP::is_dhcp_offer(const UDP::packet_t* packet)
{
    auto dhcp_packet = (packet_t*)packet->payload;
    option_t* opt = (option_t*)dhcp_packet->options;
    if ((uint8_t*)opt - packet->payload + sizeof(option_t) > packet->header.length)
        return false;
    while (opt->code != DHCP_OPT_MSG_TYPE)
    {
        opt = (option_t*)((uint8_t*)opt + opt->len + sizeof(option_t));
        if ((uint8_t*)opt - packet->payload + sizeof(option_t) > packet->header.length)
            return false;
    }

    return *opt->data == DHCP_OFFER;
}

bool DHCP::is_dhcp_ack(const UDP::packet_t* packet)
{
    auto dhcp_packet = (packet_t*)packet->payload;
    option_t* opt = (option_t*)dhcp_packet->options;
    if ((uint8_t*)opt - packet->payload + sizeof(option_t) > packet->header.length)
        return false;
    while (opt->code != DHCP_OPT_MSG_TYPE)
    {
        opt = (option_t*)((uint8_t*)opt + opt->len + sizeof(option_t));
        if ((uint8_t*)opt - packet->payload + sizeof(option_t) > packet->header.length)
            return false;
    }

    return *opt->data == DHCP_ACK;
}

size_t DHCP::get_disc_header_size()
{
    return get_header_size() + DHCP_DISC_OPT_SIZE;
}

size_t DHCP::get_request_header_size()
{
    return get_header_size() + DHCP_REQUEST_OPT_SIZE;
}
