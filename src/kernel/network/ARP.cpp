#include "ARP.h"

#include "IPV4.h"
#include "Network.h"
#include "../core/fb.h"
#include "../core/memory.h"

Ethernet::packet_info_t ARP::pending_queue[ARP_PENDING_QUEUE_SIZE] = {};
size_t ARP::pending_queue_head = 0;
ARP::cache_entry_t ARP::cache[ARP_CACHE_SIZE] = {};
size_t ARP::cache_head = 0;

void ARP::display_request(packet_t* p)
{
    uint32_t rev_srcpr = Endianness::switch32(p->srcpr);
    uint32_t rev_dstpr = Endianness::switch32(p->dstpr);
    uint16_t opcode = Endianness::switch16(p->opcode);
    printf(
        "Hardware Type: 0x%04x, Protocol Type: 0x%04x, HW Len: %d, Protocol Len: %d, Opcode: %s, Src MAC: %02x:%02x:%02x:%02x:%02x:%02x, Src IP: %d.%d.%d.%d, Dst MAC: %02x:%02x:%02x:%02x:%02x:%02x, Dst IP: %d.%d.%d.%d\n",
        p->htype, p->ptype, p->hlen, p->plen, opcode == ARP_REQUEST ? "ARP Request" : "ARP Reply",
        p->srchw[0], p->srchw[1], p->srchw[2], p->srchw[3], p->srchw[4], p->srchw[5],
        (rev_srcpr >> 24) & 0xFF, (rev_srcpr >> 16) & 0xFF, (rev_srcpr >> 8) & 0xFF, rev_srcpr & 0xFF,
        p->dsthw[0], p->dsthw[1], p->dsthw[2], p->dsthw[3], p->dsthw[4], p->dsthw[5],
        (rev_dstpr >> 24) & 0xFF, (rev_dstpr >> 16) & 0xFF, (rev_dstpr >> 8) & 0xFF, rev_dstpr & 0xFF);
}

void ARP::handlePacket(const packet_t* packet, uint8_t* response_buf, const Ethernet::packet_info* response_info)
{
    // Send response if it's an ARP request
    if (Endianness::switch16(packet->opcode) == ARP_REQUEST)
    {
        write_packet(response_buf, ARP_ETHERNET, ARP_IPV4, MAC_ADDR_LEN, IPV4_ADDR_LEN, ARP_REPLY,
                    *(uint32_t*)&Network::ip, packet->srcpr, (uint8_t*)packet->srchw);
        Network::send_packet(response_info);
    }
    else // Handle reply
    {
        // Cache the MAC address
        memcpy(&cache[cache_head].ip, &packet->srcpr, IPV4_ADDR_LEN);
        memcpy(&cache[cache_head].mac, packet->srchw, MAC_ADDR_LEN);
        cache_head  = (cache_head + 1) % ARP_CACHE_SIZE;

        // Store gateway MAC address if it's the gateway that replied
        if (memcmp(&packet->srcpr, Network::gateway_ip, IPV4_ADDR_LEN) == 0)
            memcpy(Network::gateway_mac, packet->srchw, MAC_ADDR_LEN);

        // Send packets that were waiting for this MAC address
        for (auto& packet_info : pending_queue)
        {
            if (packet_info.packet == nullptr)
                continue;

            // Check if the packet is destined to the IP address that we just resolved
            auto ip_header = (IPV4::header_t*)packet_info.packet->payload;
            if (memcmp(&ip_header->daddr, &packet->srcpr, IPV4_ADDR_LEN) != 0)
                continue;

            // Write destination MAC address and send packet
            memcpy(packet_info.packet->header.dest, &packet->srchw, MAC_ADDR_LEN);
            Network::send_packet(&packet_info);
            memset(pending_queue, 0, sizeof(pending_queue));
        }
    }
}

void ARP::resolve_gateway_mac()
{
    resolve_mac((uint8_t*)&Network::gateway_ip);
}

void ARP::resolve_and_send(const Ethernet::packet_info_t* packet_info)
{
    // Enqueue packet
    pending_queue[pending_queue_head] = *packet_info;
    pending_queue_head = (pending_queue_head + 1) % ARP_PENDING_QUEUE_SIZE;

    auto ip_header = (IPV4::header_t*)packet_info->packet->payload;
    resolve_mac((uint8_t*)&ip_header->daddr);
}

size_t ARP::get_response_size(const packet_t* packet)
{
    // check that request is targeting us
    if (!(0 == memcmp(&packet->dstpr, &Network::ip, IPV4_ADDR_LEN) ||
        0 == memcmp(&packet->dstpr, &Network::ip, sizeof(Network::broadcast_ip))))
        return -1;
    return Endianness::switch16(packet->opcode) == ARP_REQUEST ? sizeof(packet_t) : 0;
}

size_t ARP::get_headers_size()
{
    return sizeof(packet_t) + Ethernet::get_headers_size();
}

void ARP::get_mac(const uint8_t ip[IPV4_ADDR_LEN], uint8_t mac[MAC_ADDR_LEN])
{
    for (size_t i = 0; i < ARP_CACHE_SIZE; i++)
    {
        auto cache_entry = &cache[(cache_head + i) % ARP_CACHE_SIZE];
        if (memcmp(cache_entry->ip, ip, IPV4_ADDR_LEN) == 0)
        {
            memcpy(mac, cache_entry->mac, MAC_ADDR_LEN);
            return;
        }
    }

    memset(mac, 0, MAC_ADDR_LEN);
}

void ARP::resolve_mac(const uint8_t ip[IPV4_ADDR_LEN])
{
    auto headers_size = get_headers_size();
    auto packet_size = headers_size + Ethernet::get_headers_size();
    auto buf = (uint8_t*)calloc(1, packet_size);
    auto buf_beg = buf;
    buf = Ethernet::write_header(buf, (uint8_t*)Network::broadcast_mac, ETHERTYPE_ARP);
    write_packet(buf, ARP_ETHERNET, ARP_IPV4, MAC_ADDR_LEN, IPV4_ADDR_LEN, ARP_REQUEST,
                 *(uint32_t*)Network::ip, *(uint32_t*)ip, (uint8_t*)Network::broadcast_mac);

    auto packet_info = Ethernet::packet_info((Ethernet::packet_t*)buf_beg, packet_size);
    Network::send_packet(&packet_info);
}

void ARP::write_packet(uint8_t* buf, uint16_t htype, uint16_t ptype, uint8_t hlen, uint8_t plen, uint16_t opcode,
                       uint32_t srcpr, uint32_t dstpr, uint8_t dsthw[MAC_ADDR_LEN])
{
    auto packet = (packet_t*)buf;
    packet->htype = Endianness::switch16(htype);
    packet->ptype = Endianness::switch16(ptype);
    packet->hlen = hlen;
    packet->plen = plen;
    packet->opcode = Endianness::switch16(opcode);
    packet->htype = Endianness::switch16(htype);
    packet->hlen = hlen;
    packet->plen = plen;
    packet->srcpr = srcpr;
    packet->dstpr = dstpr;
    memcpy(packet->srchw, Network::mac, MAC_ADDR_LEN);
    memcpy(packet->dsthw, dsthw, MAC_ADDR_LEN);
}
