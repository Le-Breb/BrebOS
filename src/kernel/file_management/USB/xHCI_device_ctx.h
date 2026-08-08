#pragma once

#include <stdint.h>
#include <stddef.h>

#include "xHCI_common.h"

// Structures adapted from https://github.com/FlareCoding/stellux-xhci-tutorial/ (device-context-11 branch,
// which goes past the last published tutorial video and was not yet covered by any episode).

/*
// xHci Spec Section 6.2.2 Figure 6-2: Slot Context Data Structure (page 407)

The Slot Context data structure defines information that applies to a device as a whole.

Note: Figure 6-2 illustrates a 32 byte Slot Context (Context Size (CSZ) field in HCCPARAMS1 = '0').
If CSZ = '1' then each Slot Context consumes 64 bytes, where bytes 32-63 are Reserved (RsvdO).
*/
template <size_t size>
struct xhci_slot_context
{
    union
    {
        struct
        {
            // Route String - used by hubs to route packets to the correct downstream port
            uint32_t route_string    : 20;

            // Speed - deprecated, reserved. Use the PORTSC Port Speed field instead
            uint32_t speed           : 4;

            uint32_t rz              : 1;

            // Multi-TT (MTT) - set for a High-speed hub that supports Multiple TTs
            uint32_t mtt             : 1;

            // Hub - '1' if this device is a USB hub, '0' if it is a USB function
            uint32_t hub             : 1;

            // Context Entries - index of the last valid Endpoint Context in this Device Context.
            // (Context Entries + 1) * 32 bytes = total size of the Device Context structure.
            uint32_t context_entries : 5;
        };
        uint32_t dword0;
    };

    union
    {
        struct
        {
            // Worst-case time (us) to wake up all links in the path to the device
            uint16_t max_exit_latency;

            // Root Hub Port Number used to access the USB device (1-based)
            uint8_t  root_hub_port_num;

            // Number of downstream facing ports, if this device is a hub
            uint8_t  port_count;
        };
        uint32_t dword1;
    };

    union
    {
        struct
        {
            // Slot ID of the parent High-speed hub, for LS/FS devices behind it
            uint32_t parent_hub_slot_id : 8;

            // Downstream facing port number of that parent hub
            uint32_t parent_port_number : 8;

            // TT Think Time, only meaningful for High-speed hubs
            uint32_t tt_think_time      : 2;

            uint32_t rsvd0              : 4;

            // Interrupter that receives Bandwidth Request/Device Notification events for this slot
            uint32_t interrupter_target : 10;
        };
        uint32_t dword2;
    };

    union
    {
        struct
        {
            // USB Device Address assigned by the xHC upon a successful Address Device Command
            uint32_t device_address : 8;

            uint32_t rsvd1          : 19;

            /*
                Slot State, updated by the xHC:
                0 Disabled/Enabled, 1 Default, 2 Addressed, 3 Configured, 4-31 Reserved
                As Input, software shall initialize this field to '0'.
            */
            uint32_t slot_state     : 5;
        };
        uint32_t dword3;
    };

    // Bytes 10h-1Fh are for exclusive use by the xHC (Reserved and Opaque to software)
    uint32_t rsvdz[4];

    // Padding required for 64-byte context structs
    uint32_t padding[size == 64 ? 8 : 0];
} __attribute__((packed));

using xhci_slot_context32 = xhci_slot_context<32>;
using xhci_slot_context64 = xhci_slot_context<64>;

static_assert(sizeof(xhci_slot_context32) == 32, "32-byte slot context should be 32 bytes");
static_assert(sizeof(xhci_slot_context64) == 64, "64-byte slot context should be 64 bytes");

/*
// xHci Spec Section 6.2.3 Figure 6-3: Endpoint Context Data Structure (page 412)

The Endpoint Context data structure defines information that applies to a specific endpoint.
As Input, all fields shall be initialized by software before issuing a command. As Output, the
xHC updates each field to reflect the value it is currently using. Bytes 14h-1Fh are Reserved
and Opaque to software (RsvdO).
*/
template <size_t size>
struct xhci_endpoint_context
{
    union
    {
        struct
        {
            /*
                Endpoint State:
                0 Disabled, 1 Running, 2 Halted, 3 Stopped, 4 Error, 5-7 Reserved
                As Input, software shall initialize this field to '0'.
            */
            uint32_t endpoint_state        : 3;

            uint32_t rsvd0                 : 5;

            // Mult - max number of bursts within an Interval this endpoint supports (SS Isoch only)
            uint32_t mult                  : 2;

            // Max Primary Streams - '0' means the TR Dequeue Pointer references a Transfer Ring directly
            uint32_t max_primary_streams   : 5;

            // Linear Stream Array - interpretation of the Stream ID when streams are used
            uint32_t linear_stream_array   : 1;

            // Interval - period between consecutive requests, expressed as 125us * 2^Interval
            uint32_t interval              : 8;

            // High order 8 bits of Max ESIT Payload (only meaningful when LEC = '1')
            uint32_t max_esit_payload_hi   : 8;
        };
        uint32_t dword0;
    };

    union
    {
        struct
        {
            uint32_t rsvd1                 : 1;

            // Error Count (CErr) - down-counter of consecutive USB Bus Errors allowed on this endpoint's TDs
            uint32_t error_count           : 2;

            /*
                Endpoint Type:
                0 Not Valid, 1 Isoch Out, 2 Bulk Out, 3 Interrupt Out,
                4 Control (bidirectional), 5 Isoch In, 6 Bulk In, 7 Interrupt In
            */
            uint32_t endpoint_type         : 3;

            uint32_t rsvd2                 : 1;

            // Host Initiate Disable - disables Host Initiated Stream selection when set
            uint32_t host_initiate_disable : 1;

            // Max Burst Size - zero-based; 0-15 represents burst sizes of 1-16
            uint32_t max_burst_size        : 8;

            // Max Packet Size in bytes this endpoint can send/receive when configured
            uint32_t max_packet_size       : 16;
        };
        uint32_t dword1;
    };

    union
    {
        struct
        {
            // Dequeue Cycle State - the xHC Consumer Cycle State for the TRB referenced by the dequeue pointer
            uint64_t dcs                          : 1;

            uint64_t rsvd3                        : 3;

            // High order bits of the physical base address of the Transfer Ring (or Stream Context Array)
            uint64_t tr_dequeue_ptr_address_bits  : 60;
        };
        struct
        {
            uint32_t dword2;
            uint32_t dword3;
        };
        // Convenience alias: assign as (physical_base & ~0xFULL) | dcs_bit
        uint64_t transfer_ring_dequeue_ptr;
    };

    union
    {
        struct
        {
            // Average Length (bytes) of the TRBs executed by this endpoint - used for bandwidth accounting
            uint16_t average_trb_length;

            // Low order 16 bits of Max ESIT Payload (periodic endpoints only)
            uint16_t max_esit_payload_lo;
        };
        uint32_t dword4;
    };

    // Padding required for 64-byte context structs
    uint32_t padding[size == 64 ? 11 : 3];
} __attribute__((packed));

using xhci_endpoint_context32 = xhci_endpoint_context<32>;
using xhci_endpoint_context64 = xhci_endpoint_context<64>;

static_assert(sizeof(xhci_endpoint_context32) == 32, "32-byte endpoint context should be 32 bytes");
static_assert(sizeof(xhci_endpoint_context64) == 64, "64-byte endpoint context should be 64 bytes");

/*
// xHci Spec Section 6.2.1 Device Context (page 406)

Output by the xHC to report device configuration/state to software. Pointed to by an entry in
the Device Context Base Address Array (DCBAA). All unused entries shall be initialized to '0' by
software prior to the first Address Device Command that targets this slot.
*/
template <size_t size>
struct xhci_device_context
{
    xhci_slot_context<size> slot_context;
    xhci_endpoint_context<size> control_ep_context; // DCI 1
    xhci_endpoint_context<size> ep[30];              // DCI 2-31
} __attribute__((packed));

using xhci_device_context32 = xhci_device_context<32>;
using xhci_device_context64 = xhci_device_context<64>;

static_assert(sizeof(xhci_device_context32) == 1024, "32-byte device context should be 1024 bytes");
static_assert(sizeof(xhci_device_context64) == 2048, "64-byte device context should be 2048 bytes");

/*
// xHci Spec Section 6.2.5.1 Figure 6-6: Input Control Context (page 461)

Defines which Device Context data structures are affected by a command, and the operation
(add/drop) to perform on each.
*/
template <size_t size>
struct xhci_input_control_context
{
    // Drop Context flags (D2-D31) - '1' disables the respective Endpoint Context
    uint32_t drop_flags;

    // Add Context flags (A0-A31) - '1' evaluates/enables the respective Context.
    // A0 = Slot Context, A1 = Endpoint Context DCI 1 (control), A2 = DCI 2, etc.
    uint32_t add_flags;

    uint32_t rsvd[5];

    // Only used when CIC = '1' and CIE = '1' (Configuration Information Capability), unused otherwise
    uint8_t config_value;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t rsvdZ;

    // Padding required for 64-byte context structs
    uint32_t padding[size == 64 ? 8 : 0];
} __attribute__((packed));

using xhci_input_control_context32 = xhci_input_control_context<32>;
using xhci_input_control_context64 = xhci_input_control_context<64>;

static_assert(sizeof(xhci_input_control_context32) == 32, "32-byte input control context should be 32 bytes");
static_assert(sizeof(xhci_input_control_context64) == 64, "64-byte input control context should be 64 bytes");

/*
// xHci Spec Section 6.2.5 Input Context (page 459)

Specifies the endpoints and operations to be performed by the Address Device, Configure
Endpoint, and Evaluate Context Commands. The Input Context Pointer field of those Command
TRBs points at a structure like this one.
*/
template <size_t size>
struct xhci_input_context
{
    xhci_input_control_context<size> control_context;
    xhci_device_context<size> device_context;
};

using xhci_input_context32 = xhci_input_context<32>;
using xhci_input_context64 = xhci_input_context<64>;

static_assert(sizeof(xhci_input_context32) == sizeof(xhci_input_control_context32) + sizeof(xhci_device_context32));
static_assert(sizeof(xhci_input_context64) == sizeof(xhci_input_control_context64) + sizeof(xhci_device_context64));

const char* xhci_slot_state_to_string(uint8_t slot_state);
const char* xhci_ep_state_to_string(uint8_t ep_state);
