//
// Created by mat on 19/02/25.
//

#include "ARP.h"

#include "../core/fb.h"

ARP::packet* ARP::get_broadcast_request(uint8_t srchw[ARP_MAC_ADDR_LEN])
{
    uint16_t htype = ARP_ETHERNET;
    uint16_t ptype = ARP_IPV4;
    uint8_t hlen = ARP_MAC_ADDR_LEN;
    uint8_t plen = ARP_IPV4_ADDR_LEN;
    uint16_t opcode = ARP_REQUEST;
    uint8_t dsthw[ARP_MAC_ADDR_LEN] = {0,0,0,0,0,0};

    return new packet(htype, ptype, hlen, plen, opcode, srchw, 0, dsthw, 0);
}

void ARP::display_request(struct packet* p)
{
    printf("Hardware Type: 0x%04x, Protocol Type: 0x%04x, HW Len: %d, Protocol Len: %d, Opcode: %s, Src MAC: %02x:%02x:%02x:%02x:%02x:%02x, Src IP: %d.%d.%d.%d, Dst MAC: %02x:%02x:%02x:%02x:%02x:%02x, Dst IP: %d.%d.%d.%d\n",
           p->htype, p->ptype, p->hlen, p->plen, p->opcode == 1 ? "ARP Request" : "ARP Reply",
           p->srchw[0], p->srchw[1], p->srchw[2], p->srchw[3], p->srchw[4], p->srchw[5],
           (p->srcpr >> 24) & 0xFF, (p->srcpr >> 16) & 0xFF, (p->srcpr >> 8) & 0xFF, p->srcpr & 0xFF,
           p->dsthw[0], p->dsthw[1], p->dsthw[2], p->dsthw[3], p->dsthw[4], p->dsthw[5],
           (p->dstpr >> 24) & 0xFF, (p->dstpr >> 16) & 0xFF, (p->dstpr >> 8) & 0xFF, p->dstpr & 0xFF);
}
