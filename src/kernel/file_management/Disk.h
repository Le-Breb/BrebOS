#pragma once
#include <stdint.h>
#include <stddef.h>

#include "BlockDevice.h"
#include "../utils/list.h"
#include "../utils/Result.h"

#define MBR_SIGNATURE 0xAA55
#define GPT_SIGNATURE "EFI PART"


class Disk
{
public:
    enum Scheme
    {
        SuperFloppy,
        MBR,
        GPT
    };

    struct MBR_partition
    {
        uint8_t status;
        uint8_t chs_first[3];
        uint8_t type;
        uint8_t chs_last[3];
        uint32_t lba_first;
        uint32_t num_sectors;
    } __attribute__((packed));

    struct MBR_header
    {
        char bootstrap[440];
        uint32_t id;
        uint16_t rsvd;
        MBR_partition entries[4];
        uint16_t signature;
    } __attribute__((packed));

    struct Partition
    {
        uint8_t head, cylinder, sector;
        uint32_t lba_first;
        uint32_t num_sectors;
    };

    struct disk_info
    {
        Scheme partition_scheme;
        list<Partition> partitions;
    };

    static Result<Disk::disk_info> get_partitioning_info(BlockDevice* dev);
};
