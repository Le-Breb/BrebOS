#include "USB_Drive.h"

#include "FAT.h"
#include "USB/SCSI.h"

Status USB_Drive::read_blocks(uint32_t block, uint32_t num_blocks, void* buffer)
{
    TRY(SCSI::send_read_10(msd, block, num_blocks, FAT_SECTOR_SIZE, buffer));
    return Status::success();
}

Status USB_Drive::write_blocks(uint32_t block, uint32_t num_blocks, const void* buffer)
{
    TRY(SCSI::send_write_10(msd, block, num_blocks, FAT_SECTOR_SIZE, buffer));
    return Status::success();
}
