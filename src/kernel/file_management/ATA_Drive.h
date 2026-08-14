#pragma once
#include "BlockDevice.h"

class ATA_Drive : public BlockDevice
{
    static constexpr unsigned short ES = 0x10;
    const unsigned char id;
public:
    ATA_Drive(uint32_t block_size, dev_t dev, uint64_t total_size, unsigned char id)
        : BlockDevice(block_size, dev, total_size), id(id)
    {
    }

    Status read_blocks(uint32_t block, uint32_t num_blocks, void* buffer) override;
    Status write_blocks(uint32_t block, uint32_t num_blocks, const void* buffer) override;
};
