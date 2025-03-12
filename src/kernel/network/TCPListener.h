#ifndef SOCKETLISTENER_H
#define SOCKETLISTENER_H
#include <kstdint.h>


class Socket;

/**
 * Interface for any class that listen to a certain socket.
 * This allows to automatically call some callbacks when certain socket events occur
 */
class TCP_listener
{
protected:
    Socket* socket;

public:
    TCP_listener(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port);

    virtual ~TCP_listener();

    // Callback when a packet is received
    virtual void on_data_received(void* packet, uint16_t packet_size) = 0;

    // Callback when connection fails
    virtual void on_connection_error() = 0;

    virtual void on_connection_terminated() = 0;
};


#endif //SOCKETLISTENER_H
