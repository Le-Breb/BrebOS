#pragma once

#include <stdint.h>
#include <sys/stat.h>
#include "../utils/Status.h"
#include "linux/kdev_t.h"


class BlockDevice
{
    uint32_t block_size;
    dev_t dev;
    uint64_t total_size;
public:
    virtual ~BlockDevice() = default;
    BlockDevice(uint32_t block_size, dev_t dev, uint64_t total_size) : block_size(block_size), dev(dev), total_size(total_size) {}

    [[nodiscard]] uint64_t get_total_size() const { return total_size; }
    [[nodiscard]] uint32_t get_block_size() const { return block_size; }
    [[nodiscard]] uint32_t get_major() const { return MAJOR(dev); }
    [[nodiscard]] uint32_t get_minor() const { return MINOR(dev); }
    [[nodiscard]] dev_t get_dev() const { return dev; }

    virtual Status read_blocks(uint32_t block, uint32_t num_blocks, void* buffer) = 0;
    virtual Status write_blocks(uint32_t block, uint32_t num_blocks, const void* buffer) = 0;
};
