#include "ATA_Drive.h"
#include "ATA.h"

typedef unsigned int uint;

Status ATA_Drive::read_blocks(uint32_t block, uint32_t num_blocks, void* buffer)
{
    if (ATA::read_sectors(id, num_blocks, block, ES, (uint)buffer))
        return Status::failure("ATA drive read error");

    return Status::success();
}

Status ATA_Drive::write_blocks(uint32_t block, uint32_t num_blocks, const void* buffer)
{
    if (ATA::write_sectors(id, num_blocks, block, ES, (uint)buffer))
        return Status::failure("ATA drive write error");

    return Status::success();
}
