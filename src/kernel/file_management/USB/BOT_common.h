#pragma once

#include <stdint.h>

// USB Mass Storage Class - Bulk-Only Transport (BOT), Revision 1.0
// https://www.usb.org/sites/default/files/usbmassbulk_10.pdf
//
// dCBWSignature/dCSWSignature and every other
// multi-byte field in the CBW/CSW are little-endian on the wire, matching this CPU's native
// byte order, so they can be filled in directly with no byte-swapping (unlike the SCSI CDBs
// themselves - see SCSI_common.h - which are big-endian and need Endianness::switch16/32,
// same as this codebase's network headers already do).

/*
// Section 3.1: Command Block Wrapper (CBW)

Sent host -> device on the bulk OUT endpoint immediately before every command (and, if
data_transfer_length > 0, before the data stage that follows it).
*/
#define BOT_CBW_SIGNATURE  0x43425355u // 'USBC', already correct as a plain uint32_t on this (little-endian) CPU

// bmCBWFlags bit 7: direction of the data stage that follows, if any (ignored/must be 0 if
// data_transfer_length == 0)
#define BOT_CBW_FLAGS_DIRECTION_OUT  0x00 // Host to device
#define BOT_CBW_FLAGS_DIRECTION_IN   0x80 // Device to host

// bCBWCBLength: cb[] is a fixed 16-byte field, but only its first cb_length bytes are
// significant. All the SCSI CDBs in SCSI_common.h are <= 16 bytes.
#define BOT_CBWCB_MAX_LENGTH  16

struct bot_cbw
{
    uint32_t signature;                // Always BOT_CBW_SIGNATURE
    uint32_t tag;                      // Opaque value chosen by the host; echoed back in the matching CSW
    uint32_t data_transfer_length;     // Bytes the host expects to move in the data stage (0 if none)
    uint8_t  flags;                    // BOT_CBW_FLAGS_DIRECTION_*
    uint8_t  lun;                      // Bits 3:0 = target Logical Unit Number (0 for a single-LUN device)
    uint8_t  cb_length;                // Valid bytes in `cb` (1-16)
    uint8_t  cb[BOT_CBWCB_MAX_LENGTH]; // The SCSI CDB (see SCSI_common.h), zero-padded past cb_length
} __attribute__((packed));
static_assert(sizeof(bot_cbw) == 31);

/*
// Section 3.2: Command Status Wrapper (CSW)

Received device -> host on the bulk IN endpoint once the command (and its data stage, if any)
has completed. Always read exactly 13 bytes for this, on the same bulk IN endpoint used for an
IN data stage (there's only one bulk IN pipe).
*/
#define BOT_CSW_SIGNATURE  0x53425355u // 'USBS'

#define BOT_CSW_STATUS_PASSED       0 // Command completed successfully
#define BOT_CSW_STATUS_FAILED       1 // Command failed - check sense data via REQUEST SENSE
#define BOT_CSW_STATUS_PHASE_ERROR  2 // Protocol violation - device expects a full reset (BOT_REQUEST_RESET)

struct bot_csw
{
    uint32_t signature;     // Always BOT_CSW_SIGNATURE
    uint32_t tag;           // Echoes the bot_cbw::tag of the command this status belongs to - verify it matches
    uint32_t data_residue;  // data_transfer_length minus bytes actually transferred in the data stage
    uint8_t  status;        // BOT_CSW_STATUS_*
} __attribute__((packed));
static_assert(sizeof(bot_csw) == 13);

/*
// Section 3.3: Class-Specific Requests

Sent to the mass storage Interface via the default control pipe (xHCI::control_transfer), not
over the bulk endpoints. wIndex is the interface number (usb_mass_storage_device::interface_number).
*/

// Device-to-host | Class | Interface. wValue = 0, wLength = 1. Returns 1 byte: the highest LUN
// index the device supports (0 means "1 LUN, numbered 0").
#define BOT_REQUEST_GET_MAX_LUN       0xFE
#define BOT_REQUEST_TYPE_GET_MAX_LUN  0xA1

// Host-to-device | Class | Interface. wValue = 0, wLength = 0, no data stage. Per spec this must
// be followed by CLEAR_FEATURE(ENDPOINT_HALT) on both bulk endpoints before resuming traffic.
#define BOT_REQUEST_RESET       0xFF
#define BOT_REQUEST_TYPE_RESET  0x21
