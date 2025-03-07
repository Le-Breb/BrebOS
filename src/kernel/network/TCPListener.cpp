#include "Socket.h"
#include "TCPListener.h"

TCP_listener::TCP_listener(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port) : socket(new Socket{peer_ip, peer_port})
{
    socket->set_listener(this);
}

TCP_listener::~TCP_listener()
{
    delete socket;
}
