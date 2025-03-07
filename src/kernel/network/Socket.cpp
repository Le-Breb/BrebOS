#include "Socket.h"

#include "Endianness.h"
#include "Network.h"
#include "../core/fb.h"

list<Socket*> Socket::sockets = {};
uint16_t Socket::socket_head = 0;

void Socket::acknowledge(const TCP::header_t* packet, uint8_t* response_buf)
{
    ack_num = Endianness::switch32(packet->seq_num) + 1;
    TCP::write_ack_header(response_buf, this);
}

void Socket::flush_waiting_queue() // Todo: take peer window size into account
{
    for (uint i = 0; i < waiting_queue_size; i++)
    {
        auto idx = (waiting_queue_start + i) % WAITING_QUEUE_CAPACITY;
        auto packet = waiting_queue[idx];
        send_data(packet->data, packet->size);
        waiting_queue[idx] = nullptr;
    }
}

Socket::Socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port) : peer_port(peer_port)
{
    memcpy(ip, Network::ip, IPV4_ADDR_LEN);
    memcpy(this->peer_ip, peer_ip, IPV4_ADDR_LEN);

    while (port_used(port))
        port = TCP::random_ephemeral_port();

    sockets.add(this);
}

void Socket::set_listener(TCP_listener* listener)
{
    this->listener = listener;
}

Socket::~Socket()
{
    sockets.remove(this);
}

void Socket::initialize()
{
    state = TCP::State::SYN_SENT;
    TCP::send_sync(this);
}

void Socket::handle_packet(const TCP::header_t* packet, uint8_t* response_buf, uint16_t packet_size,
                           const Ethernet::packet_info_t* response_info)
{
    auto flags = packet->flags;
    if (flags == (TCP_FLAG_SYN | TCP_FLAG_ACK) && state == TCP::State::SYN_SENT) // SYN response
    {
        seq_num++;
        acknowledge(packet, response_buf);
        state = TCP::State::ESTABLISHED;
        Network::send_packet(response_info);
        printf("State is established\n");
        flush_waiting_queue();
    }
    else if (flags == TCP_FLAG_ACK && state == TCP::State::ESTABLISHED)
        return;
    else if (flags == (TCP_FLAG_FIN | TCP_FLAG_ACK) && state == TCP::State::FIN_WAIT_1)
    {
        acknowledge(packet, response_buf);
        Network::send_packet(response_info);
        state = TCP::State::CLOSED;
    }
    else if (flags == (TCP_FLAG_PSH | TCP_FLAG_ACK) && state == TCP::State::ESTABLISHED)
    {
        acknowledge(packet, response_buf);
        Network::send_packet(response_info);
        if (listener != nullptr)
        {
            auto header_size = packet->header_len * sizeof(uint32_t);
            auto payload = (uint8_t*)packet + header_size;
            auto payload_size = packet_size - header_size;
            listener->on_data_received(payload, payload_size);
        }
    }
    else // Not handled, force close connection
    {
        TCP::write_reset_header(response_buf, this);
        Network::send_packet(response_info);
    }
}

Socket* Socket::port_used(uint16_t port)
{
    for (int i = 0; i < sockets.size(); i++)
    {
        auto ssocket = sockets.get(i);
        if (!ssocket)
            printf_error("wtf\n");
        auto socket = *ssocket;
        if (socket->port == port)
            return socket;
    }

    return nullptr;
}

void Socket::close_all_connections()
{
    bool connection_open = true;
    while (connection_open)
    {
        connection_open = false;
        for (int i = 0; i < sockets.size(); i++)
        {
            auto socket = *sockets.get(i);
            connection_open = connection_open || (socket->state != TCP::State::CLOSED);
            if (socket->state != TCP::State::ESTABLISHED)
                continue;
            TCP::send_fin_ack(socket);
        }
        Network::pollPackets();
    }
}

Socket* Socket::reset_response_socket(uint8_t peer_ip[4], uint16_t peer_port, uint32_t peer_seq_num)
{
    auto socket = new Socket(peer_ip, peer_port);
    socket->ack_num = peer_seq_num + 1;
    return socket;
}

void Socket::send_data(const void* data, uint16_t size)
{
    if (state == TCP::State::CLOSED)
    {
        if (waiting_queue_size == WAITING_QUEUE_CAPACITY)
        {
            printf_error("Socket waiting queue is full\n");
            return;
        }
        waiting_queue[(waiting_queue_start + waiting_queue_size++) % WAITING_QUEUE_CAPACITY] = new packet_t{data, size};
        initialize();
    }
    else
    {
        TCP::send_data(data, size, this);
        seq_num += size;
        delete[] (char*)data;
    }
}
