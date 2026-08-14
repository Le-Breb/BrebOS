#pragma once
#include "BlockDevice.h"
#include "USB/USB.h"


class USB_Drive : public BlockDevice
{
    usb_mass_storage_device* msd;
public:
    explicit USB_Drive(uint32_t block_size, dev_t dev, uint32_t total_size,
                       usb_mass_storage_device* device) : BlockDevice(block_size, dev, total_size), msd(device)
    {
    }

    Status read_blocks(uint32_t block, uint32_t num_blocks, void* buffer) override;
    Status write_blocks(uint32_t block, uint32_t num_blocks, const void* buffer) override;
};
