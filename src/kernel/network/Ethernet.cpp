#include "Ethernet.h"

#include "Endianness.h"

void Ethernet::write_header(header_t* ptr, uint8_t dest[ETHERNET_MAC_LEN], uint8_t src[ETHERNET_MAC_LEN], uint16_t type)
{
    memcpy(ptr->dest, dest, ETHERNET_MAC_LEN);
    memcpy(ptr->src, src, ETHERNET_MAC_LEN);
    ptr->type = Endianness::switch_endian16(type);
}
