#include "Disk.h"

#include "../utils/Result.h"

Result<Disk::disk_info> Disk::get_partitioning_info(BlockDevice* dev)
{
    // Constants
    const uint64_t total_size = dev->get_total_size();
    const uint32_t block_size = dev->get_block_size();
    const uint32_t num_blocks = total_size / block_size;

    // Unique partition in case of superfloppy
    list<Partition> superfloppy_partitions;
    superfloppy_partitions.add({
        .head = 0,
        .cylinder = 0,
        .sector = 1,
        .lba_first = 0,
        .num_sectors = num_blocks
    });

    const uint num_blocks_to_read = sizeof(MBR_header) / block_size + 1;
    if (total_size < num_blocks_to_read * block_size) // Disk too small to contain partitions
        return make_ok(disk_info{SuperFloppy, superfloppy_partitions});

    // Read header
    unsigned char* buf = new unsigned char[num_blocks_to_read * block_size];
    if (const auto read_res = dev->read_blocks(0, num_blocks_to_read, buf); !read_res.is_ok())
    {
        delete[] buf;
        return MAKE_ERR("Failed to read MBR from device %u:%u. Reason: %s", dev->get_major(), dev->get_minor(), read_res.err().what());
    }

    // Superfloppy if no MBR signature
    const MBR_header* mbr = reinterpret_cast<const MBR_header*>(buf);
    if (mbr->signature != MBR_SIGNATURE)
        return make_ok(disk_info{SuperFloppy, superfloppy_partitions});

    // Protective MBR convention: partition entry 0 has type 0xEE
    if (mbr->entries[0].type == 0xEE)
    {
        printf_warn("GPT partition scheme detected. GPT is not supported yet, ignoring partitions");
        return make_ok(disk_info{GPT, superfloppy_partitions});
    }

    // At this point we're left with something that looks like MBR. We must ensure it is actually MBR by sanity checking
    // the entries
    bool has_valid_entry = false;
    for (const auto& entry : mbr->entries)
    {
        if ((entry.status == 0x00 || entry.status == 0x80) && entry.num_sectors != 0)
        {
            has_valid_entry = true;
            break;
        }
    }

    if (!has_valid_entry)
        return make_ok(disk_info{SuperFloppy, superfloppy_partitions});

    disk_info info{};
    info.partition_scheme = MBR;
    for (const auto& entry : mbr->entries)
    {
        if (entry.type != 0)
        {
            Partition partition{};
            partition.head = entry.chs_first[0];
            partition.cylinder = ((entry.chs_first[1] & 0xC0) << 2) | entry.chs_first[2];
            partition.sector = entry.chs_first[1] & 0x3F;
            partition.lba_first = entry.lba_first;
            partition.num_sectors = entry.num_sectors;
            info.partitions.add(partition);
        }
    }

    return make_ok(info);
}
