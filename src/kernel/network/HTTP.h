#ifndef HTTP_H
#define HTTP_H

#include <kstdint.h>

#include "NetworkConsts.h"
#include "TCPListener.h"
#include "../utils/list.h"
#include "../utils/progress_bar_user.h"

#define DIGIT(c) (c >= '0' && c <= '9')

#define HTTP_OK 200
#define HTTP_MOVED_PERMANENTLY 301
#define HTTP_SERVICE_UNAVAILABLE 503

class HTTP : TCP_listener, Progress_bar_user
{
public:
    enum class State
    {
        CLOSED,
        GET_SENT,
        RECEIVING_RESPONSE,
        RESPONSE_COMPLETE
    };

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

    typedef struct request request_t;

    struct response
    {
        char* method{};
        int status{};
        char* reason{};
        header_t* headers{};
        uint16_t num_headers{};

        /**
         * Builds a response from a response packet, struct is zeroed out
         * On error (ie size is too short),
         * @param packet response packet
         * @param packet_size length of the packet
         */
        response(const void* packet, uint16_t packet_size);

        header_t* get_header(const char* name) const;
    };

    typedef struct response response_t;

    HTTP(const char* hostname, uint16_t peer_port);

    void send_get(const char* uri);

    void on_data_received(void* packet, uint16_t packet_size) override;

    ~HTTP() override;

private:
    static list<HTTP*> instances;
    uint8_t* buf = nullptr;
    uint16_t buf_capacity = 0;
    uint16_t buf_size = 0;
    request_t* request = nullptr;
    response_t* response = nullptr;

    [[nodiscard]] static void* create_packet(const request_t* request, uint16_t& packet_size);

    [[nodiscard]] static size_t get_request_size(const request_t* request);

    State state = State::CLOSED;

    static uint16_t atoi(const char* str);

    /**
     * Counts the number of headers
     * @param header_beg pointer to beginning of the first header
     * @param len length of the buffer
     * @return number of headers, -1 on error
     */
    [[nodiscard]] static uint16_t count_headers(const char* header_beg, uint16_t len);

    /**
     * Parses a status code
     * @param str status string
     * @return status, 0 on error
     */
    static int parse_status_code(const char str[3]);

    /**
     * Searches for a character in a string
     * @param str string to search into
     * @param c character to find
     * @param len length of the string
     * @return pointer to the character, nullptr if not found
     */
    [[nodiscard]] static const char* find_char(const char* str, char c, uint16_t len);

    static void display_response(const response_t* response);

    void handle_response_descriptor(const void* packet, uint16_t packet_size);

    // Socket listener callback
    void on_connection_terminated(const char* error_message) override;

    static void close_instance(HTTP* instance);
};


#endif //HTTP_H
