#pragma once

#include <stdint.h>

// Small USB-level (not xHCI-specific) structures shared between xHCI.h and USB.h.
// Kept in their own header so xHCI.h doesn't have to include USB.h (which includes xHCI.h).

/*
// USB 2.0 Spec Section 9.3: USB Device Requests

The 8-byte SETUP packet sent at the start of every control transfer. This is placed verbatim
(not via a pointer) into the "parameter" field of a Setup Stage TRB - see xHci Spec Section
6.4.1.2.1.
*/
struct usb_device_request
{
    uint8_t  bm_request_type;
    uint8_t  b_request;
    uint16_t w_value;
    uint16_t w_index;
    uint16_t w_length;
} __attribute__((packed));
static_assert(sizeof(usb_device_request) == 8);

/*
// USB 2.0 Spec Section 9.6.6: Standard Endpoint Descriptor (relevant fields only)

Fed to xHCI::configure_endpoints() once the caller has parsed a Configuration Descriptor and
knows which endpoint(s) it wants to bring up.
*/
struct usb_endpoint_desc
{
    uint8_t  address;          // bEndpointAddress: bit 7 = direction (1 = IN), bits 3:0 = endpoint number
    uint8_t  attributes;       // bmAttributes: bits 1:0 = transfer type (0 Control, 1 Isoch, 2 Bulk, 3 Interrupt)
    uint16_t max_packet_size;  // wMaxPacketSize, bits 10:0 = size in bytes
    uint8_t  xhci_interval;    // Endpoint Context "Interval" value already converted from bInterval (spec 6.2.3.6)
};

/*
// USB 2.0 Spec Section 9.4: Standard Device Requests

Requests/values needed to read descriptors and select a configuration during enumeration.
*/
#define USB_REQUEST_GET_DESCRIPTOR     6
#define USB_REQUEST_SET_CONFIGURATION  9

// USB 2.0 Spec Table 9-5: Descriptor Types (high byte of GET_DESCRIPTOR's wValue)
#define USB_DESCRIPTOR_TYPE_DEVICE         1
#define USB_DESCRIPTOR_TYPE_CONFIGURATION  2
#define USB_DESCRIPTOR_TYPE_STRING         3
#define USB_DESCRIPTOR_TYPE_INTERFACE      4
#define USB_DESCRIPTOR_TYPE_ENDPOINT       5

// USB 2.0 Spec Section 9.6.7: default (US English) Language ID, used to fetch String Descriptors
// when a device's own supported-languages list (String Descriptor index 0) can't be read
#define USB_LANGID_US_ENGLISH  0x0409

// USB Mass Storage Class Spec: class/subclass/protocol identifying a SCSI-over-Bulk-Only-Transport interface
#define USB_CLASS_MASS_STORAGE     0x08
#define USB_SUBCLASS_SCSI          0x06
#define USB_PROTOCOL_BULK_ONLY     0x50

// USB 2.0 Spec Table 9-13: bmAttributes bits 1:0 (Endpoint Descriptor) - Transfer Type
#define USB_ENDPOINT_TRANSFER_TYPE_BULK  2

/*
// USB 2.0 Spec Section 9.6.2 Table 9-8: Standard Device Descriptor
*/
struct usb_device_descriptor
{
    uint8_t  length;
    uint8_t  descriptor_type;
    uint16_t bcd_usb;
    uint8_t  device_class;
    uint8_t  device_sub_class;
    uint8_t  device_protocol;
    uint8_t  max_packet_size0;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t bcd_device;
    uint8_t  manufacturer_idx;
    uint8_t  product_idx;
    uint8_t  serial_number_idx;
    uint8_t  num_configurations;
} __attribute__((packed));
static_assert(sizeof(usb_device_descriptor) == 18);

/*
// USB 2.0 Spec Section 9.6.3 Table 9-10: Standard Configuration Descriptor (header only - the full
// descriptor returned by the device is followed by that configuration's Interface/Endpoint/class
// descriptors, back to back, for a combined total of total_length bytes)
*/
struct usb_config_descriptor
{
    uint8_t  length;
    uint8_t  descriptor_type;
    uint16_t total_length;
    uint8_t  num_interfaces;
    uint8_t  configuration_value;
    uint8_t  configuration_idx;
    uint8_t  attributes;
    uint8_t  max_power;
} __attribute__((packed));
static_assert(sizeof(usb_config_descriptor) == 9);

/*
// USB 2.0 Spec Section 9.6.5 Table 9-12: Standard Interface Descriptor
*/
struct usb_interface_descriptor
{
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_sub_class;
    uint8_t interface_protocol;
    uint8_t interface_idx;
} __attribute__((packed));
static_assert(sizeof(usb_interface_descriptor) == 9);

/*
// USB 2.0 Spec Section 9.6.6 Table 9-13: Standard Endpoint Descriptor (raw wire format, as returned
// inside a Configuration Descriptor - not to be confused with usb_endpoint_desc above, which is the
// trimmed-down struct xHCI::configure_endpoints() actually takes)
*/
struct usb_endpoint_descriptor_raw
{
    uint8_t  length;
    uint8_t  descriptor_type;
    uint8_t  endpoint_address;
    uint8_t  attributes;
    uint16_t max_packet_size;
    uint8_t  interval;
} __attribute__((packed));
static_assert(sizeof(usb_endpoint_descriptor_raw) == 7);
