#include "HTTP.h"

#include <kstring.h>

#include "Socket.h"
#include "TCP.h"
#include "../core/fb.h"
#include "../core/memory.h"
#include "../file_management/VFS.h"
#include <kstring.h>

list<HTTP*> HTTP::instances = {};

uint16_t HTTP::request::get_size() const
{
    auto size = strlen(method) + strlen(uri) + strlen(version) + 6; //  for 3 * '\r' + '\n'
    for (uint16_t i = 0; i < num_headers; i++)
    {
        auto h = headers[i];
        size += strlen(h.name) + strlen(h.value) + 4; // +4 for ':', ' ', '\r', '\n'
    }

    return size + 2; // +2 for trailing '\r' + '\n'
}

HTTP::response::response(const void* packet, uint16_t packet_size) : method(nullptr), status(0), reason(nullptr),
                                                                     headers(nullptr), num_headers(0)
{
    auto cpacket = (const char*)packet;
    uint16_t remaining_size = packet_size;

    const char* method_end = find_char(cpacket, ' ', packet_size);
    if (method_end == nullptr)
        return;

    // Get method
    uint16_t method_len = method_end - cpacket;
    method = new char[method_len + 1];
    method[method_len] = '\0';
    memcpy(method, cpacket, method_len);

    // Update pointers and sizes
    remaining_size -= method_len + 1; // + ' '
    cpacket += method_len + 1;

    // Get status code
    if (remaining_size + 3 > packet_size)
    {
        delete[] method;
        return;
    }
    status = parse_status_code(cpacket);
    if (status == 0)
    {
        delete[] method;
        return;
    }

    // Update pointers and sizes
    remaining_size -= 3 + 1; // + ' '
    cpacket += 3 + 1;

    // Get reason
    const char* reason_end = find_char(cpacket, '\r', remaining_size);
    if (reason_end == nullptr)
    {
        delete[] method;
        status = 0;
        return;
    }
    uint16_t reason_len = reason_end - cpacket;
    reason = new char[reason_len + 1];
    memcpy(reason, cpacket, reason_len);

    // Update pointers and sizes
    remaining_size -= reason_len + 2; // + \r + \n
    cpacket += reason_len + 2;

    // Get num headers
    num_headers = count_headers(cpacket, remaining_size);
    if (num_headers == (uint16_t)-1)
    {
        delete[] method;
        status = 0;
        delete[] reason;
        return;
    }

    if (num_headers == 0)
        return;

    // Get headers
    headers = new header_t[num_headers];
    for (uint16_t i = 0; i < num_headers; i++)
    {
        // Get header name
        const char* header_name_end = find_char(cpacket, ':', remaining_size);
        uint16_t header_name_len = header_name_end - cpacket;
        headers[i].name = new char[header_name_len + 1];
        headers[i].name[header_name_len] = '\0';
        memcpy(headers[i].name, cpacket, header_name_len);

        // Update pointers and sizes
        cpacket += header_name_len + 1; // + ':'
        remaining_size -= header_name_len + 1;

        // Get header value
        const char* header_value_end = find_char(cpacket, '\r', remaining_size);
        uint16_t header_value_len = header_value_end - cpacket;
        headers[i].value = new char[header_value_len + 1];
        headers[i].value[header_value_len] = '\0';
        bool starts_with_space = header_name_len && *cpacket == ' ';
        memcpy(headers[i].value, cpacket + starts_with_space, header_value_len - starts_with_space);
        if (starts_with_space)
            headers[i].value[header_value_len - 1] = '\0';

        // Update pointers and sizes
        cpacket += header_value_len + 2; // + \r + \n
        remaining_size -= header_value_len + 2;
    }
}

HTTP::header_t* HTTP::response::get_header(const char* name) const
{
    for (uint16_t i = 0; i < num_headers; i++)
    {
        const char* n = name; // name pointer
        const char* hn = headers[i].name; // header name pointer
        char nc[2] = {}; // curr carr in name stored in single character string
        char hc[2] = {}; // curr carr in header name stored in single character string

        while (*n && *hn)
        {
            // Get char in name and header name
            nc[0] = *n;
            hc[0] = *hn;
            // Ensure lowercase is used
            to_lower_in_place(nc);
            to_lower_in_place(hc);
            // If characters do not match exit
            if (nc[0] != hc[0])
                break;
            // Proceed to next char
            n++;
            hn++;
        }
        // If we did not reach the end of both name and header name, they are !=, proceed to next header
        if (*n || *hn)
            continue;

        // Header name matched, return it
        return headers + i;
    }

    return nullptr;
}

HTTP::HTTP(const char* hostname, uint16_t peer_port): TCP_listener(hostname, peer_port)
{
    instances.add(this);
}

void HTTP::display_response(const response_t* response)
{
    printf("%s %u %s\n", response->method, response->status, response->reason);
    for (uint16_t i = 0; i < response->num_headers; i++)
        printf("%s: %s\n", response->headers[i].name, response->headers[i].value);
}

void HTTP::send_get(const char* uri)
{
    request = new request_t();

    request->method = (char*)"GET";
    request->uri = new char[strlen(uri) + 1];
    strcpy(request->uri, uri);
    request->version = (char*)"HTTP/1.1";
    constexpr uint16_t num_headers = 4;
    request->num_headers = num_headers;
    header_t headers[num_headers]{};
    headers[0].name = (char*)"Host";
    headers[0].value = (char*)socket->get_hostname();
    headers[1].name = (char*)"User-Agent";
    headers[1].value = (char*)"BrebOS";
    headers[2].name = (char*)"Accept";
    headers[2].value = (char*)"*/*";
    headers[3].name = (char*)"Connection";
    headers[3].value = (char*)"close";
    request->headers = headers;

    uint16_t packet_size;
    void* packet = create_packet(request, packet_size);

    state = State::GET_SENT;

    printf_info("Downloading %s from %s:%u", uri, socket->get_hostname(), socket->get_peer_port());
    socket->send_data(packet, packet_size);
}

void HTTP::on_data_received(void* packet, uint16_t packet_size)
{
    switch (state)
    {
        case State::GET_SENT: // Get response
            handle_response_descriptor(packet, packet_size);
            break;
        case State::RECEIVING_RESPONSE: // Get response body
            if (buf_size + packet_size > buf_capacity)
            {
                socket->close();
                printf_error("size of received data exceed buffer capacity");
                state = State::CLOSED;
            }
            memcpy(buf + buf_size, packet, packet_size);
            buf_size += packet_size;
            printf("Received %u/%u bytes\n", buf_size, buf_capacity);
            break;
        default:
            printf("HTTP packet received while on state %d\n", (int)state);
            socket->close();
            break;
    }
}

void HTTP::handle_response_descriptor(const void* packet, uint16_t packet_size)
{
    response = new response_t(packet, packet_size);

    if (response->method == nullptr)
    {
        printf("malformed HTTP packet received\n");
        socket->close();
        state = State::CLOSED;
        return;
    }
    if (!(state == State::GET_SENT && response->status == HTTP_OK))
    {
        if (response->status == HTTP_MOVED_PERMANENTLY)
        {
            auto location_header = response->get_header("Location");
            printf_warn("Redirected to %s. This is likely to happen for websites that redirect HTTP to HTTPS",
                location_header->value);
        }
        else if (response->status == HTTP_SERVICE_UNAVAILABLE)
            printf_warn("Service is unavailable");
        else
            printf_warn("Incoherent HTTP response or status is not OK\n");
        socket->close();
        state = State::CLOSED;
        return;
    }

    auto content_length_header = response->get_header("Content-Length");
    if (content_length_header == nullptr)
    {
        printf("No content length header\n");
        socket->close();
        state = State::CLOSED;
        return;
    }
    size_t content_len_len = 0;
    while (content_length_header->value[content_len_len])
    {
        buf_capacity *= 10;
        buf_capacity += content_length_header->value[content_len_len] - '0';
        content_len_len++;
    }

    // Create a buffer to store response body
    buf = new uint8_t[buf_capacity];

    state = State::RECEIVING_RESPONSE;

    // Gather remaining data
    uint16_t i = 0; // Skip HTTP response
    char* cpacket = (char*)packet;
    while (i < packet_size - 3 && !(cpacket[i] == '\r' && cpacket[i + 1] == '\n' && cpacket[i + 2] == '\r' &&
           cpacket[i + 3] == '\n'))
        i++;

    // Copy remaining data
    auto data_size = packet_size - i - 4; // -4 for \r\n\r\n
    memcpy(buf, cpacket + i + 4, data_size);

    buf_size = data_size;

    if (buf_size)
        printf("Received %u/%u bytes\n", buf_size, buf_capacity);
}

void HTTP::on_connection_terminated(const char* error_message)
{
    if (error_message)
        printf_warn("HTTP error: %s", error_message);
    else
    {
        // Response body completely received
        if (buf_size == buf_capacity && state == State::RECEIVING_RESPONSE)
        {
            state = State::RESPONSE_COMPLETE;

            // Build file path
            const char save_path[] = "/downloads/";
            const char* file_name = "res.txt"; // request->uri;
            auto pathname_length = sizeof(save_path) + strlen(file_name);
            char pathname[pathname_length + 1];
            pathname[pathname_length] = '\0';
            strcpy(pathname, save_path);
            strcat(pathname, file_name);

            // Save file
            if (VFS::write_buf_to_file(pathname, buf, buf_size))
                printf_info("%s downloaded and saved at %s", request->uri, pathname);
            else
                printf_error("Error while saving downloaded file %s at %s", request->uri, pathname);
        }
        else
            printf_error("HTTP closed with incomplete response: %d/%d", buf_size, buf_capacity);
    }

    state = State::CLOSED;
    delete[] buf;
    buf_size = 0;
    buf_capacity = 0;

    close_instance(this);
    // Do not write any code here as 'this' is deleted at this point!
}

void HTTP::close_instance(HTTP* instance)
{
    instances.remove(instance);
    delete instance;
}

const char* HTTP::find_char(const char* str, char c, uint16_t len)
{
    const char* beg = str;
    while (str - beg <= len && *str != c)
        str++;

    return str - beg > len ? nullptr : str;
}

uint16_t HTTP::count_headers(const char* header_beg, uint16_t len)
{
    if (len < 4)
        return -1;
    uint16_t c = 0;
    const char* window_start = header_beg;
    while (window_start - header_beg <= len)
    {
        if (window_start[0] == '\r' && window_start[1] == '\n')
        {
            c++;
            if (window_start[2] == '\r' && window_start[3] == '\n')
                return c;
        }
        window_start++;
    }

    return -1; // Double \r \n not found, error
}

int HTTP::parse_status_code(const char str[3])
{
    if (!DIGIT(str[0]) || !DIGIT(str[1]) || !DIGIT(str[2]))
        return 0;

    return (str[0] - '0') * 100 + (str[1] - '0') * 10 + str[2] - '0';
}

void* HTTP::create_packet(const request_t* request, uint16_t& packet_size)
{
    packet_size = get_request_size(request);
    auto buf = (char*)calloc(packet_size, 1);
    auto buf_beg = buf;

    // Write method
    auto method_len = strlen(request->method);
    memcpy(buf, request->method, method_len);
    buf += method_len;
    *buf++ = ' ';

    // Write uri
    auto uri_len = strlen(request->uri);
    memcpy(buf, request->uri, uri_len);
    buf += uri_len;
    *buf++ = ' ';

    // Write version
    auto version_len = strlen(request->version);
    memcpy(buf, request->version, version_len);
    buf += version_len;
    *buf++ = '\r';
    *buf++ = '\n';

    // Write headers
    for (auto i = 0; i < request->num_headers; i++)
    {
        auto header = &request->headers[i];

        // Write header name
        auto header_name_len = strlen(header->name);
        memcpy(buf, header->name, header_name_len);
        buf += header_name_len;

        // Write separator
        *buf++ = ':';
        *buf++ = ' ';

        // Write header value
        auto header_value_len = strlen(header->value);
        memcpy(buf, header->value, header_value_len);
        buf += header_value_len;

        // Write end of header
        *buf++ = '\r';
        *buf++ = '\n';
    }

    // Write end of headers
    *buf++ = '\r';
    *buf++ = '\n';

    return buf_beg;
}

size_t HTTP::get_request_size(const request_t* request)
{
    size_t size = strlen(request->method) + 1 + strlen(request->uri) + 1 + strlen(request->version) + 2;
    // +2 for 2 * ' ' and +2 for \r\n

    for (auto i = 0; i < request->num_headers; i++)
    {
        auto header = &request->headers[i];
        size += strlen(header->name) + strlen(header->value) + 4; // +4 for :' ' and \r\n
    }

    return size + 2;
}

uint16_t HTTP::atoi(const char* str)
{
    uint16_t ret = 0;

    while (*str)
    {
        ret *= 10;
        ret += *str - '0';
        str++;
    }

    return ret;
}

HTTP::~HTTP()
{
    delete request;
    delete response;
    instances.remove(this);
}
