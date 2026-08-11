#include "BOT.h"

#include <cstring>

uint32_t BOT::tag_ctr = 0;

Status BOT::check_csw_against_cbw(const bot_csw& csw, const bot_cbw& cbw)
{
    if (csw.signature != BOT_CSW_SIGNATURE)
        return Status::failure("Invalid CSW signature");
    if (csw.tag != cbw.tag)
        return Status::failure("CSW tag mismatch");
    if (csw.status != 0)
        return Status::failure("Command failed with status 0x%x", csw.status);

    return Status::success();
}

Status BOT::send_in(const usb_mass_storage_device* device, const cdb_t& cdb, void* data, size_t length)
{
    xHCI* xHCI = xHCI::get_instance();

    bot_cbw cbw{};
    cbw.signature = BOT_CBW_SIGNATURE;
    cbw.tag = tag_ctr++;
    cbw.data_transfer_length = length;
    cbw.flags = BOT_CBW_FLAGS_DIRECTION_IN;
    cbw.lun = 0;
    cbw.cb_length = cdb.length;
    memcpy(cbw.cb, cdb.bytes, cdb.length);

    if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_out_endpoint, &cbw, sizeof(cbw)); !s.is_ok())
        return Status::failure("Failed to send CBW: %s", s.err().what());

    if (length > 0) {
        if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_in_endpoint, data, length); !s.is_ok())
            return Status::failure("Failed to read data stage: %s", s.err().what());
    }

    bot_csw csw{};
    if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_in_endpoint, &csw, sizeof(csw)); !s.is_ok())
        return Status::failure("Failed to read CSW: %s", s.err().what());

    return check_csw_against_cbw(csw, cbw);
}

Status BOT::send_out(const usb_mass_storage_device* device, const cdb_t& cdb, void* data, size_t length)
{
    xHCI* xHCI = xHCI::get_instance();

    bot_cbw cbw{};
    cbw.signature = BOT_CBW_SIGNATURE;
    cbw.tag = tag_ctr++;
    cbw.data_transfer_length = length;
    cbw.flags = BOT_CBW_FLAGS_DIRECTION_OUT;
    cbw.lun = 0;
    cbw.cb_length = cdb.length;
    memcpy(cbw.cb, cdb.bytes, cdb.length);

    if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_out_endpoint, &cbw, sizeof(cbw)); !s.is_ok())
        return Status::failure("Failed to send CBW: %s", s.err().what());

    if (length > 0) {
        if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_out_endpoint, data, length); !s.is_ok())
            return Status::failure("Failed to send data stage: %s", s.err().what());
    }

    bot_csw csw{};
    if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_in_endpoint, &csw, sizeof(csw)); !s.is_ok())
        return Status::failure("Failed to read CSW: %s", s.err().what());

    return check_csw_against_cbw(csw, cbw);
}
