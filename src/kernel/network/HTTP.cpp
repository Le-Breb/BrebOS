#include "HTTP.h"

#include <kstring.h>

#include "Socket.h"
#include "TCP.h"
#include "../core/fb.h"
#include "../core/memory.h"

uint16_t HTTP::request::get_size() const
{
    auto size = strlen(method) + strlen(uri) + strlen(version) + 6;
    for (uint16_t i = 0; i < num_headers; i++)
    {
        auto h = headers[i];
        size += strlen(h.name) + strlen(h.value) + 4;
    }

    return size + 2;
}

HTTP::HTTP(uint8_t peer_ip[4], uint16_t peer_port): TCP_listener(peer_ip, peer_port)
{
}

void HTTP::send_get(const char* uri) const
{
    request_t request{};
    request.method = (char*)"GET";
    request.uri = (char*)"/";
    request.version = (char*)"HTTP/1.1";
    constexpr uint16_t num_headers = 3;
    request.num_headers = num_headers;
    header_t headers[num_headers]{};
    headers[0].name = (char*)"Host";
    headers[0].value = (char*)uri;
    headers[1].name = (char*)"User-Agent";
    headers[1].value = (char*)"BrebOS";
    headers[2].name = (char*)"Accept";
    headers[2].value = (char*)"*/*";
    request.headers = headers;

    uint16_t packet_size;
    void* packet = create_packet(&request, packet_size);
    socket->send_data(packet, packet_size);
}

void HTTP::on_data_received(void* packet, uint16_t packet_size)
{
    printf("HTTP packet received: \n");
    for (uint16_t i = 0; i < packet_size; i++)
        printf("%02x", ((uint8_t*)packet)[i]);
    printf("\n");
}

void* HTTP::create_packet(const request_t* request, uint16_t& packet_size)
{

    packet_size = get_request_size(request);
    auto buf = (char*)calloc(packet_size, 1);
    auto buf_beg = buf;
    auto method_len = strlen(request->method);
    memcpy(buf, request->method, method_len);
    buf += method_len;
    *buf++ = ' ';
    auto uri_len = strlen(request->uri);
    memcpy(buf, request->uri, uri_len);
    buf += uri_len;
    *buf++ = ' ';
    auto version_len = strlen(request->version);
    memcpy(buf, request->version, version_len);
    buf += version_len;\
    *buf++ = '\r';
    *buf++ = '\n';

    for (auto i = 0; i < request->num_headers; i++)
    {
        auto header = &request->headers[i];
        auto header_name_len = strlen(header->name);
        memcpy(buf, header->name, header_name_len);
        buf += header_name_len;
        *buf++ = ':';
        *buf++ = ' ';
        auto header_value_len = strlen(header->value);
        memcpy(buf, header->value, header_value_len);
        buf += header_value_len;
        *buf++ = '\r';
        *buf++ = '\n';
    }

    *buf++ = '\r';
    *buf++ = '\n';

    return buf_beg;
}

size_t HTTP::get_request_size(const request_t* request)
{
    size_t size = strlen(request->method) + 1 + strlen(request->uri) + 1 + strlen(request->version) + 2;

    for (auto i = 0; i < request->num_headers; i++)
    {
        auto header = &request->headers[i];
        size += strlen(header->name) + strlen(header->value) + 4; // +4 for :' ' and \r\n
    }

    return size + 2;
}