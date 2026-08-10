#pragma once
#include "SCSI_common.h"
#include "USB.h"
#include "../../utils/Result.h"

class SCSI
{
public:
    static Result<scsi_read_capacity_10_data> send_read_capacity_10(const usb_mass_storage_device* device);
    static Status send_read_10(const usb_mass_storage_device* device, uint32_t lba, uint16_t transfer_length,
                                uint32_t block_length, void* buffer);
    static Status send_write_10(const usb_mass_storage_device* device, uint32_t lba, uint16_t transfer_length,
                                 uint32_t block_length, const void* buffer);
};
