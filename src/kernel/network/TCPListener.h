#ifndef SOCKETLISTENER_H
#define SOCKETLISTENER_H
#include <kstdint.h>


class Socket;

class TCP_listener
{
protected:
    Socket* socket;

public:
    TCP_listener(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port);

    virtual ~TCP_listener();

    virtual void on_data_received(void* packet, uint16_t packet_size) = 0;

    virtual void on_connection_error() = 0;
};


#endif //SOCKETLISTENER_H
