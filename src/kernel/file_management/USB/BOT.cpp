#include "BOT.h"

#include <cstring>
#include "../../core/memory/memory.h"

uint32_t BOT::tag_ctr = 0;

/*
The xHC DMAs directly from/to the CBW and CSW, so they must live in physically contiguous memory
whose physical address is reliably resolvable. A plain stack buffer guarantees neither (it can
straddle a page boundary, leaving the tail at an unrelated physical address). physically_aligned_malloc
gives us that, but has no matching deallocator and hands out whole pages - so allocate these once and
reuse them rather than leaking a page per command. Safe because the driver only ever has a single
transfer in flight (same assumption as xHCI::send_command_trb/wait_for_transfer_event).
*/
static bot_cbw* cbw_dma_buf = nullptr;
static bot_csw* csw_dma_buf = nullptr;

static bot_cbw* get_cbw_dma_buf()
{
    if (!cbw_dma_buf)
        cbw_dma_buf = static_cast<bot_cbw*>(Memory::physically_aligned_malloc(sizeof(bot_cbw), 8, PAGE_SIZE));

    return cbw_dma_buf;
}

static bot_csw* get_csw_dma_buf()
{
    if (!csw_dma_buf)
        csw_dma_buf = static_cast<bot_csw*>(Memory::physically_aligned_malloc(sizeof(bot_csw), 8, PAGE_SIZE));

    return csw_dma_buf;
}

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

    bot_cbw* cbw = get_cbw_dma_buf();
    bot_csw* csw = get_csw_dma_buf();
    if (!cbw || !csw)
        return Status::failure("Failed to allocate CBW/CSW DMA buffers");

    memset(cbw, 0, sizeof(bot_cbw));
    cbw->signature = BOT_CBW_SIGNATURE;
    cbw->tag = tag_ctr++;
    cbw->data_transfer_length = length;
    cbw->flags = BOT_CBW_FLAGS_DIRECTION_IN;
    cbw->lun = 0;
    cbw->cb_length = cdb.length;
    memcpy(cbw->cb, cdb.bytes, cdb.length);

    if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_out_endpoint, cbw, sizeof(bot_cbw)); !s.is_ok())
        return Status::failure("Failed to send CBW: %s", s.err().what());

    if (length > 0) {
        if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_in_endpoint, data, length); !s.is_ok())
            return Status::failure("Failed to read data stage: %s", s.err().what());
    }

    memset(csw, 0, sizeof(bot_csw));
    if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_in_endpoint, csw, sizeof(bot_csw)); !s.is_ok())
        return Status::failure("Failed to read CSW: %s", s.err().what());

    return check_csw_against_cbw(*csw, *cbw);
}

Status BOT::send_out(const usb_mass_storage_device* device, const cdb_t& cdb, void* data, size_t length)
{
    xHCI* xHCI = xHCI::get_instance();

    bot_cbw* cbw = get_cbw_dma_buf();
    bot_csw* csw = get_csw_dma_buf();
    if (!cbw || !csw)
        return Status::failure("Failed to allocate CBW/CSW DMA buffers");

    memset(cbw, 0, sizeof(bot_cbw));
    cbw->signature = BOT_CBW_SIGNATURE;
    cbw->tag = tag_ctr++;
    cbw->data_transfer_length = length;
    cbw->flags = BOT_CBW_FLAGS_DIRECTION_OUT;
    cbw->lun = 0;
    cbw->cb_length = cdb.length;
    memcpy(cbw->cb, cdb.bytes, cdb.length);

    if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_out_endpoint, cbw, sizeof(bot_cbw)); !s.is_ok())
        return Status::failure("Failed to send CBW: %s", s.err().what());

    if (length > 0) {
        if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_out_endpoint, data, length); !s.is_ok())
            return Status::failure("Failed to send data stage: %s", s.err().what());
    }

    memset(csw, 0, sizeof(bot_csw));
    if (const Status s = xHCI->bulk_transfer(device->device, device->bulk_in_endpoint, csw, sizeof(bot_csw)); !s.is_ok())
        return Status::failure("Failed to read CSW: %s", s.err().what());

    return check_csw_against_cbw(*csw, *cbw);
}
