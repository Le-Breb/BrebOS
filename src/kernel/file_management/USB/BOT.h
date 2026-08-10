#pragma once
#include "BOT_common.h"
#include "USB.h"
#include "../../utils/Status.h"
#include <utility>

struct cdb_t {
    uint8_t bytes[16];
    uint8_t length;
};

template <typename T>
concept CDB = sizeof(T) <= sizeof(std::declval<cdb_t>().bytes);

class BOT
{
    BOT() = default;
    static uint32_t tag_ctr;

    static Status check_csw_against_cbw(const bot_csw& csw, const bot_cbw& cbw);
public:
    // Get data from device. data will be filled by the device.
    static Status send_in(const usb_mass_storage_device* device, const cdb_t& cdb, void* data, size_t length);
    static Status send_out(const usb_mass_storage_device* device, const cdb_t& cdb, void* data, size_t length);

    template <CDB T>
    static cdb_t build_cdb(const T& cdb);
};

template <CDB T>
cdb_t BOT::build_cdb(const T& cdb)
{
    cdb_t result{};
    memcpy(result.bytes, &cdb, sizeof(cdb));
    result.length = sizeof(cdb);
    return result;
}

