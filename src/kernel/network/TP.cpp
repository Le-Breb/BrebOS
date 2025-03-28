#include "TP.h"

#include "../core/fb.h"

#include "Endianness.h"
#include "Network.h"

bool TP::request_sent = false;

uint8_t TP::server_ip[IPV4_ADDR_LEN] = {132, 163, 97, 6}; // time.nist.gov

uint32_t TP::time = 0;

#define EPOCH_OFFSET 2208988800U

// Days in months (for non-leap years)
static const uint days_in_month[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

bool is_leap_year(uint year)
{
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

// Convert Unix timestamp to human-readable date (YYYY-MM-DD HH:MM:SS)
void convert_to_local_time(uint32_t unix_timestamp)
{
    // Subtract the epoch offset to get seconds since Unix epoch
    uint32_t timestamp = unix_timestamp - EPOCH_OFFSET;

    uint seconds = timestamp % 60;
    uint minutes = (timestamp / 60) % 60;
    uint hours = (timestamp / 3600) % 24;
    uint days = timestamp / 86400;

    // Calculate the year, month, and day
    uint year = 1970;
    while (days >= (is_leap_year(year) ? 366 : 365))
    {
        days -= (is_leap_year(year) ? 366 : 365);
        year++;
    }

    uint month = 0;
    while (days >= days_in_month[month] + (month == 1 && is_leap_year(year)))
    {
        days -= days_in_month[month] + (month == 1 && is_leap_year(year));
        month++;
    }

    // Month is 0-indexed, so adjust to the usual 1-12 format
    month += 1;

    // Print the result in a human-readable format
    printf("Time: %04d-%02d-%02d %02d:%02d:%02d\n", year, month, days + 1, hours, minutes, seconds);
}

bool TP::handle_packet(const UDP::packet_t* packet)
{
    if (Endianness::switch16(packet->header.length) != UDP::get_header_size() + sizeof(uint32_t))
        return false;

    if (!request_sent)
        return false;

    uint32_t time_value = Endianness::switch32(*(uint32_t*)packet->payload);
    convert_to_local_time(time_value);
    time = time_value;

    return true;
}

void TP::send_request()
{
    Ethernet::packet_info_t packet_info;
    UDP::create_packet(Network::random_ephemeral_port(), TP_DST_PORT, 0,
        *(uint32_t*)server_ip, Network::gateway_mac, packet_info);
    request_sent = true;

    Network::send_packet(&packet_info);
}
