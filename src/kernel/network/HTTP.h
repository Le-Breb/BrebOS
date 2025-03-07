#ifndef HTTP_H
#define HTTP_H

#include <kstdint.h>
#include <kstddef.h>

#include "NetworkConsts.h"
#include "TCPListener.h"

class HTTP : TCP_listener
{
public:
    struct header
    {
        char* name;
        char* value;
    };

    typedef struct header header_t;

    struct request
    {
        char* method;
        char* uri;
        char* version;
        header_t* headers;
        uint16_t num_headers;

        [[nodiscard]] uint16_t get_size() const;
    };

    HTTP(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port);

    typedef struct request request_t;

    void send_get(const char* uri) const;

    void on_data_received(void* packet, uint16_t packet_size) override;

    private:
    [[nodiscard]] static void* create_packet(const request_t* request, uint16_t& packet_size);

    [[nodiscard]] static size_t get_request_size(const request_t* request);

public:
    void on_connection_error() override;
};


#endif //HTTP_H
