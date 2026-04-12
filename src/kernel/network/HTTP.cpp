#include "HTTP.h"

#include <kstddef.h>

#include "Socket.h"
#include "TCP.h"
#include "../core/fb.h"
#include "../core/memory.h"
#include "../file_management/VFS.h"

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

HTTP::HTTP(const char* hostname, uint16_t peer_port): TCP_listener(hostname, peer_port), Progress_bar_user()
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
    request->uri = (char*)"/";
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

void HTTP::handle_response_continuation(void* packet, uint16_t packet_size)
{
    if (buf_size + packet_size > buf_capacity)
    {
        // Chunked transfers are headers\r\nlength\r\npayload\r\n
        // If buf_size + packet_size > buf_capacity, its probably because it's the end of a chunked response, as
        // 'length' does not account for the trailing \r\n. If it's the case, simply remove 2 to packet size and
        // proceed normally
        if (transfer_type == TransferType::Chunked &&
            buf_size + packet_size == buf_capacity + 2 && memcmp((char*)packet + packet_size - 2, "\r\n", 2) == 0)
        {
            if (current_chunk_size == 0)
            {
                state = State::RESPONSE_COMPLETE;
                return;
            }
            packet_size -= 2;
            state = State::RECEIVING_CHUNK_LEN;
        }
        else {
            printf_error("size of received data exceeds buffer capacity");
            socket->close();
            state = State::CLOSED;
            return;
        }
    }
    memcpy(buf + buf_size, packet, packet_size);
    buf_size += packet_size;
    update_progress(buf_size, buf_capacity);

    if (transfer_type == TransferType::Regular && buf_size == buf_capacity)
        state = State::RESPONSE_COMPLETE;
}

/** Gets chunk length and store it in current_chunk_length
 *
 * @param packet received packet
 * @param packet_size size of the received packet
 * @return beginning address of the chunk
 */
void* HTTP::handle_chunk_length(void* packet, uint16_t packet_size)
{
    constexpr char end_of_len_signature[] = "\r\n";
    void* end_of_chunk_len = memmem(packet, packet_size, end_of_len_signature, sizeof(end_of_len_signature) - 1);

    if (end_of_chunk_len == nullptr)
    {
        printf_error("Couldn't find chunk length");
        socket->close();
        state = State::CLOSED;
        return nullptr;
    }

    size_t chunk_len_len = (char*)end_of_chunk_len - (char*)packet;
    current_chunk_size = 0;
    for (size_t i = 0; i < chunk_len_len;  i++)
    {
        current_chunk_size *= 16;
        current_chunk_size += ((char*)packet)[i] - '0';
    }

    return (char*)end_of_chunk_len + sizeof(end_of_len_signature) - 1;
}

void HTTP::on_data_received(void* packet, uint16_t packet_size)
{
    switch (state)
    {
        case State::GET_SENT: // Get response
            handle_response_descriptor(packet, packet_size);
            break;
        case State::RECEIVING_CHUNK_LEN:
        {
            if (void* content = handle_chunk_length(packet, packet_size))
            {
                uint16_t content_size = packet_size - ((char*)content - (char*)packet);
                handle_response_continuation(content, content_size);
            }

            break;
        }
        case State::RECEIVING_RESPONSE: // Get response body
        {
            handle_response_continuation(packet, packet_size);
            break;
        }
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

    char* cpacket = (char*)packet;
    void* end_of_headers = nullptr;
    constexpr char end_of_headers_signature[] = "\r\n\r\n";
    if ((end_of_headers = memmem(cpacket, packet_size, end_of_headers_signature, sizeof(end_of_headers_signature) - 1)) == nullptr)
    {
        printf_error("Couldn't find end of HTTP headers");
        socket->close();
        state = State::CLOSED;
        return;
    }
    end_of_headers = (char*)end_of_headers + sizeof(end_of_headers_signature) - 1;
    size_t payload_size = packet_size - ((char*)end_of_headers - cpacket);
    size_t content_len = 0;
    void* content_start = end_of_headers;

    auto content_length_header = response->get_header("Content-Length");
    if (content_length_header == nullptr)
    {
        // No content length provided: check if response is chunked
        if (auto transfer_type_header = response->get_header("Transfer-Encoding"))
        {
            if (strcmp(transfer_type_header->value, "chunked") != 0)
            {
                printf_error("Unsupported transfer type: %s\n", transfer_type_header->value);
                socket->close();
                state = State::CLOSED;
                return;
            }

            state = State::RECEIVING_CHUNK_LEN;
            transfer_type = TransferType::Chunked;

            // Get chunk length length
            constexpr char end_of_content_len_len_signature[] = "\r\n";
            void* end_of_content_len = memmem(end_of_headers, payload_size, end_of_content_len_len_signature, sizeof(end_of_content_len_len_signature) - 1);
            if (end_of_content_len == nullptr)
            {
                printf_error("Couldn't find HTTP chunk length");
                socket->close();
                state = State::CLOSED;
                return;
            }
            size_t content_len_len = (char*)end_of_content_len - (char*)end_of_headers;
            // Get chunk length
            for (size_t i = 0; i < content_len_len; i++)
            {
                content_len *= 16;
                content_len += ((char*)end_of_headers)[i] - '0';
            }
            current_chunk_size = content_len;
            content_start = (char*)end_of_content_len + sizeof(end_of_content_len_len_signature) - 1;
        }
        else
        {
            printf("No content length header\n");
            socket->close();
            state = State::CLOSED;
        }
    }
    else
    {
        size_t content_len_len = 0;
        while (content_length_header->value[content_len_len])
        {
            content_len *= 10;
            content_len += content_length_header->value[content_len_len] - '0';
            content_len_len++;
        }
    }

    state = State::RECEIVING_RESPONSE;

    // Create a buffer to store response body
    buf_capacity = content_len;
    buf = new uint8_t[buf_capacity];

    // Copy remaining data. It's not necessarily content_len bytes long as chunked responses can be split by TCP
    auto data_size = packet_size - ((char*)content_start - (char*)packet);
    memcpy(buf, content_start, data_size);

    buf_size = data_size;

    if (buf_size)
        update_progress(buf_size, buf_capacity);
    //printf("Received %u/%u bytes\n", buf_size, buf_capacity);
}

void HTTP::on_connection_terminated(const char* error_message)
{
    if (error_message)
        printf_warn("HTTP error: %s", error_message);
    else
    {
        // Response body completely received
        if (buf_size == buf_capacity && state == State::RESPONSE_COMPLETE)
        {
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
