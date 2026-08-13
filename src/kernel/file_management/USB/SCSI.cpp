#include "SCSI.h"

#include "BOT.h"
#include "../../utils/Endianness.h"
#include "../../core/memory/memory.h"
#include "kstring.h"

/*
The xHC DMAs directly into this buffer, so it needs to be a reliably DMA-safe allocation rather
than a plain stack buffer (see BOT::send_in/send_out). physically_aligned_malloc has no matching
deallocator and hands out whole pages, so allocate it once and reuse it rather than leaking a page
per command. Safe because the driver only ever has a single transfer in flight.
*/

static scsi_read_capacity_10_data* get_read_capacity_10_dma_buf()
{
    static scsi_read_capacity_10_data* read_capacity_10_dma_buf = nullptr;
    if (!read_capacity_10_dma_buf)
        read_capacity_10_dma_buf = static_cast<scsi_read_capacity_10_data*>(
            Memory::physically_aligned_malloc(sizeof(scsi_read_capacity_10_data), 8, PAGE_SIZE));

    return read_capacity_10_dma_buf;
}

Result<scsi_read_capacity_10_data> SCSI::send_read_capacity_10(const usb_mass_storage_device* device)
{
    scsi_cdb_read_capacity_10 cdb;
    cdb.opcode = SCSI_OP_READ_CAPACITY_10;
    cdb.reserved0 = 0;
    cdb.lba = 0; // Basic form, get capacity of the whole LUN
    cdb.reserved1[0] = 0;
    cdb.reserved1[1] = 0;
    cdb.pmi = 0; // Basic form, leave 0
    cdb.control = 0;

    cdb_t bot_cdb;
    memcpy(bot_cdb.bytes, &cdb, sizeof(cdb));
    bot_cdb.length = sizeof(cdb);

    scsi_read_capacity_10_data* data = get_read_capacity_10_dma_buf();
    if (!data)
        return MAKE_ERR("Failed to allocate READ CAPACITY (10) response buffer");

    if (const Status send_status = BOT::send_in(device, bot_cdb, data, sizeof(*data)); !send_status.is_ok())
        return MAKE_ERR("Failed to send READ CAPACITY (10) command: %s", send_status.err().what());

    data->last_lba = Endianness::switch32(data->last_lba);
    data->block_length = Endianness::switch32(data->block_length);

    return Result<scsi_read_capacity_10_data>::ok(*data);
}

Status SCSI::send_read_10(const usb_mass_storage_device* device, uint32_t lba, uint16_t transfer_length,
                           uint32_t block_length, void* buffer)
{
    scsi_cdb_read_10 cdb;
    cdb.opcode = SCSI_OP_READ_10;
    cdb.flags = 0; // Plain read
    cdb.lba = Endianness::switch32(lba);
    cdb.group_number = 0;
    cdb.transfer_length = Endianness::switch16(transfer_length);
    cdb.control = 0;

    const size_t data_length = static_cast<size_t>(transfer_length) * block_length;
    if (const Status send_status = BOT::send_in(device, BOT::build_cdb(cdb), buffer, data_length); !send_status.is_ok())
        return Status::failure("Failed to send READ (10) command: %s", send_status.err().what());

    return Status::success();
}

Status SCSI::send_write_10(const usb_mass_storage_device* device, uint32_t lba, uint16_t transfer_length,
    uint32_t block_length, const void* buffer)
{
    scsi_cdb_write_10 cdb;
    cdb.opcode = SCSI_OP_WRITE_10;
    cdb.flags = 0; // Plain write
    cdb.lba = Endianness::switch32(lba);
    cdb.group_number = 0;
    cdb.transfer_length = Endianness::switch16(transfer_length);
    cdb.control = 0;

    const size_t data_length = static_cast<size_t>(transfer_length) * block_length;
    if (const Status send_status = BOT::send_out(device, BOT::build_cdb(cdb), (void*)buffer, data_length); !send_status.is_ok())
        return Status::failure("Failed to send WRITE (10) command: %s", send_status.err().what());

    return Status::success();
}
