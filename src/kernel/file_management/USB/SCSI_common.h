#pragma once

#include <stdint.h>

// SCSI Primary Commands (SPC) / SCSI Block Commands (SBC) - just the subset a Bulk-Only
// Transport flash drive driver typically needs. Struct/constant definitions only - no logic.
//
// Every multi-byte field below (LBA, lengths, ...) is BIG-ENDIAN on the wire, same as this
// codebase's network headers (see network/Endianness.h). Fill/read them the same way those
// do it, e.g.:
//   cdb.lba = Endianness::switch32(lba);
//   uint32_t lba = Endianness::switch32(cdb.lba);
// This is the opposite convention from BOT_common.h's CBW/CSW, which are little-endian.

// SPC-4 Table 142: Operation codes for the commands defined below
#define SCSI_OP_TEST_UNIT_READY   0x00
#define SCSI_OP_REQUEST_SENSE     0x03
#define SCSI_OP_INQUIRY           0x12
#define SCSI_OP_READ_CAPACITY_10  0x25
#define SCSI_OP_READ_10           0x28
#define SCSI_OP_WRITE_10          0x2A

/*
// SPC-4 Section 6.47: TEST UNIT READY (6 bytes)

Simplest possible command - just checks whether the LUN is ready to accept media-access
commands. A GOOD status means ready; CHECK CONDITION (see bot_csw::status) means not ready or
needs attention - follow up with REQUEST SENSE to find out why.
*/
struct scsi_cdb_test_unit_ready
{
    uint8_t opcode; // SCSI_OP_TEST_UNIT_READY
    uint8_t reserved[4];
    uint8_t control;
} __attribute__((packed));
static_assert(sizeof(scsi_cdb_test_unit_ready) == 6);

/*
// SPC-4 Section 6.35: REQUEST SENSE (6 bytes)

Issued right after a command comes back with CHECK CONDITION status, to retrieve the sense
data (see scsi_sense_data_fixed below) explaining what went wrong.
*/
struct scsi_cdb_request_sense
{
    uint8_t opcode;             // SCSI_OP_REQUEST_SENSE
    uint8_t desc;               // bit 0: 0 = fixed format sense data (scsi_sense_data_fixed), 1 = descriptor format
    uint8_t reserved[2];
    uint8_t allocation_length;  // Max bytes of sense data to return (18 is enough for fixed format)
    uint8_t control;
} __attribute__((packed));
static_assert(sizeof(scsi_cdb_request_sense) == 6);

/*
// SPC-4 Section 6.6: INQUIRY (6 bytes)

Identifies the device (vendor/product strings, device type). Set evpd = 0 and page_code = 0 to
get the Standard INQUIRY Data (scsi_inquiry_data below).
*/
#define SCSI_INQUIRY_EVPD  0x01 // evpd bit 0: request Vital Product Data page `page_code` instead of standard data

struct scsi_cdb_inquiry
{
    uint8_t  opcode;             // SCSI_OP_INQUIRY
    uint8_t  evpd;                // SCSI_INQUIRY_EVPD or 0
    uint8_t  page_code;           // VPD page, only meaningful if evpd is set
    uint16_t allocation_length;   // Big-endian. 36 is enough for the fixed part of scsi_inquiry_data
    uint8_t  control;
} __attribute__((packed));
static_assert(sizeof(scsi_cdb_inquiry) == 6);

/*
// SBC-3 Section 5.15: READ CAPACITY (10) (10 bytes)

Leave lba = 0 and pmi = 0 (the "basic form") to get the capacity of the whole LUN back in
scsi_read_capacity_10_data.
*/
struct scsi_cdb_read_capacity_10
{
    uint8_t  opcode;      // SCSI_OP_READ_CAPACITY_10
    uint8_t  reserved0;
    uint32_t lba;         // Big-endian. Leave 0 unless pmi is set
    uint8_t  reserved1[2];
    uint8_t  pmi;          // bit 0: Partial Medium Indicator - leave 0
    uint8_t  control;
} __attribute__((packed));
static_assert(sizeof(scsi_cdb_read_capacity_10) == 10);

/*
// SBC-3 Section 5.10 / 5.32: READ (10) / WRITE (10) (10 bytes each)

transfer_length is a block count, not a byte count - multiply by the LUN's block size (from
scsi_read_capacity_10_data::block_length) to get bot_cbw::data_transfer_length.
*/
struct scsi_cdb_read_10
{
    uint8_t  opcode;           // SCSI_OP_READ_10
    uint8_t  flags;             // DPO/FUA/RARC/... - 0 for a plain read
    uint32_t lba;               // Big-endian, first logical block to read
    uint8_t  group_number;      // 0
    uint16_t transfer_length;   // Big-endian, number of logical blocks to read
    uint8_t  control;
} __attribute__((packed));
static_assert(sizeof(scsi_cdb_read_10) == 10);

struct scsi_cdb_write_10
{
    uint8_t  opcode;           // SCSI_OP_WRITE_10
    uint8_t  flags;             // DPO/FUA/... - 0 for a plain write
    uint32_t lba;               // Big-endian, first logical block to write
    uint8_t  group_number;      // 0
    uint16_t transfer_length;   // Big-endian, number of logical blocks to write
    uint8_t  control;
} __attribute__((packed));
static_assert(sizeof(scsi_cdb_write_10) == 10);

/*
// SPC-4 Section 6.6.2 Table 141: Standard INQUIRY Data (fixed 36-byte part - the part every
// device implements; some devices append vendor-specific data past this, which callers can
// ignore)
*/
#define SCSI_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS_BLOCK  0x00 // what a flash drive/HDD reports

struct scsi_inquiry_data
{
    uint8_t peripheral;             // bits 4:0 = device type (SCSI_PERIPHERAL_DEVICE_TYPE_*), bits 7:5 = qualifier
    uint8_t removable;              // bit 7 = RMB (removable medium)
    uint8_t version;
    uint8_t response_data_format;   // bits 3:0 = response data format, other bits = NORMACA/HISUP/obsolete
    uint8_t additional_length;      // Number of bytes following this one (31 for the fixed part)
    uint8_t flags0;
    uint8_t flags1;
    uint8_t flags2;
    char    vendor_id[8];           // ASCII, space-padded, NOT null-terminated
    char    product_id[16];         // ASCII, space-padded, NOT null-terminated
    char    product_revision[4];    // ASCII, space-padded, NOT null-terminated
} __attribute__((packed));
static_assert(sizeof(scsi_inquiry_data) == 36);

/*
// SBC-3 Section 5.16.2: READ CAPACITY (10) parameter data (8 bytes)

last_lba is the address of the LAST valid logical block, not a block count - the LUN's total
block count is last_lba + 1.
*/
struct scsi_read_capacity_10_data
{
    uint32_t last_lba;      // Big-endian
    uint32_t block_length;  // Big-endian, bytes per logical block (512 for the vast majority of sticks)
} __attribute__((packed));
static_assert(sizeof(scsi_read_capacity_10_data) == 8);

/*
// SPC-4 Section 4.5.3 Table 47: Fixed Format Sense Data (18 bytes)

What REQUEST SENSE returns (with desc = 0). sense_key is the field to check first: it sorts
the error into a broad category (SCSI_SENSE_KEY_*); additional_sense_code/qualifier narrow it
down further (ASC/ASCQ pairs are cataloged in the SPC spec, e.g. 0x3A/0x00 = "medium not
present").
*/
#define SCSI_SENSE_KEY_NO_SENSE         0x0
#define SCSI_SENSE_KEY_RECOVERED_ERROR  0x1
#define SCSI_SENSE_KEY_NOT_READY        0x2
#define SCSI_SENSE_KEY_MEDIUM_ERROR     0x3
#define SCSI_SENSE_KEY_HARDWARE_ERROR   0x4
#define SCSI_SENSE_KEY_ILLEGAL_REQUEST  0x5
#define SCSI_SENSE_KEY_UNIT_ATTENTION   0x6 // Commonly returned once after a device is first connected/reset
#define SCSI_SENSE_KEY_DATA_PROTECT     0x7
#define SCSI_SENSE_KEY_BLANK_CHECK      0x8
#define SCSI_SENSE_KEY_ABORTED_COMMAND  0xB
#define SCSI_SENSE_KEY_VOLUME_OVERFLOW  0xD
#define SCSI_SENSE_KEY_MISCOMPARE       0xE

struct scsi_sense_data_fixed
{
    uint8_t  response_code;                  // bits 6:0 - 0x70 (current errors) or 0x71 (deferred errors)
    uint8_t  obsolete;
    uint8_t  sense_key;                      // bits 3:0 = SCSI_SENSE_KEY_*, bits 7:5 = FILEMARK/EOM/ILI flags
    uint32_t information;                    // Big-endian, command-specific (e.g. LBA of a medium error)
    uint8_t  additional_sense_length;        // Bytes following this one (10 for this fixed-size struct)
    uint32_t command_specific_information;   // Big-endian
    uint8_t  additional_sense_code;          // ASC
    uint8_t  additional_sense_code_qualifier;// ASCQ
    uint8_t  field_replaceable_unit_code;
    uint8_t  sense_key_specific[3];
} __attribute__((packed));
static_assert(sizeof(scsi_sense_data_fixed) == 18);
