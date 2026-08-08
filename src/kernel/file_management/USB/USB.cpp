#include "USB.h"

#include "../../core/memory/memory.h"
#include <kstring.h>

__attribute__ ((format (printf, 1, 2))) extern int printf_warn(const char* format, ...);
__attribute__ ((format (printf, 1, 2))) extern int printf_info(const char* format, ...);

// USB 2.0 Spec Section 9.6.7: fetches the device's supported-languages String Descriptor
// (index 0) and returns its first Language ID, falling back to US English if that fails.
static uint16_t get_usb_string_lang_id(xHCI* xhci, const SharedPointer<xhci_device>& device)
{
    void* buf = Memory::physically_aligned_malloc(255, 8, PAGE_SIZE);
    if (!buf)
        return USB_LANGID_US_ENGLISH;

    const usb_device_request req = {
        .bm_request_type = 0x80, // Device-to-host | Standard | Device
        .b_request = USB_REQUEST_GET_DESCRIPTOR,
        .w_value = static_cast<uint16_t>(USB_DESCRIPTOR_TYPE_STRING << 8), // String Descriptor index 0
        .w_index = 0,
        .w_length = 255
    };

    uint32_t actual = 0;
    if (!xhci->control_transfer(device, req, buf, &actual) || actual < 4)
        return USB_LANGID_US_ENGLISH;

    const auto* raw = static_cast<const uint8_t*>(buf);
    return static_cast<uint16_t>(raw[2] | (raw[3] << 8));
}

/*
// USB 2.0 Spec Section 9.6.7: String Descriptor

Fetches String Descriptor `index` (an iProduct/iManufacturer/... field from another descriptor,
NOT a string itself) and decodes its UTF-16LE payload into ASCII, replacing any non-ASCII code
point with '?'. Leaves out[0] = '\0' and returns false if index is 0 (device has no such string)
or the descriptor couldn't be read.
*/
static bool get_usb_string(xHCI* xhci, const SharedPointer<xhci_device>& device, uint8_t index,
                            uint16_t lang_id, char* out, size_t out_size)
{
    out[0] = '\0';
    if (index == 0)
        return false;

    void* buf = Memory::physically_aligned_malloc(255, 8, PAGE_SIZE);
    if (!buf)
        return false;

    const usb_device_request req = {
        .bm_request_type = 0x80,
        .b_request = USB_REQUEST_GET_DESCRIPTOR,
        .w_value = static_cast<uint16_t>((USB_DESCRIPTOR_TYPE_STRING << 8) | index),
        .w_index = lang_id,
        .w_length = 255
    };

    uint32_t actual = 0;
    if (!xhci->control_transfer(device, req, buf, &actual) || actual < 2)
        return false;

    const auto* raw = static_cast<const uint8_t*>(buf);
    const uint8_t desc_len = raw[0] < actual ? raw[0] : static_cast<uint8_t>(actual);
    if (desc_len < 2)
        return false;

    const size_t char_count = (desc_len - 2) / 2;

    size_t n = 0;
    for (; n < char_count && n + 1 < out_size; n++)
    {
        const uint16_t code_unit = static_cast<uint16_t>(raw[2 + n * 2] | (raw[3 + n * 2] << 8));
        out[n] = code_unit < 128 ? static_cast<char>(code_unit) : '?';
    }
    out[n] = '\0';

    return n > 0;
}

USB* USB::get_instance()
{
    static USB* instance = nullptr;

    if (instance)
        return instance;
    if (!xHCI::get_instance())
    {
        printf_warn("Cannot get USB instance as no xCHI controller is available");
        return nullptr;
    }
    return new USB();
}

void USB::enumerate_devices()
{
    for (const auto& device : xHCI::get_instance()->get_devices())
    {
        bool already_processed = false;
        for (const auto slot : processed_slots)
            if (slot == device->get_slot())
            {
                already_processed = true;
                break;
            }

        if (already_processed)
            continue;

        enumerate_device(device);
        processed_slots.push_back(device->get_slot());
    }
}

void USB::enumerate_device(const SharedPointer<xhci_device>& device)
{
    xHCI* xhci = xHCI::get_instance();
    const uint8_t slot = device->get_slot();

    // --- Full Device Descriptor (osdev enumeration step 7, part 1) ---
    void* dev_desc_buf = Memory::physically_aligned_malloc(sizeof(usb_device_descriptor), 8, PAGE_SIZE);
    if (!dev_desc_buf)
    {
        printf_warn("USB: failed to allocate device descriptor buffer for slot %i", slot);
        return;
    }

    const usb_device_request get_dev_desc_req = {
        .bm_request_type = 0x80, // Device-to-host | Standard | Device
        .b_request = USB_REQUEST_GET_DESCRIPTOR,
        .w_value = static_cast<uint16_t>(USB_DESCRIPTOR_TYPE_DEVICE << 8),
        .w_index = 0,
        .w_length = sizeof(usb_device_descriptor)
    };

    uint32_t actual = 0;
    if (!xhci->control_transfer(device, get_dev_desc_req, dev_desc_buf, &actual) || actual < sizeof(usb_device_descriptor))
    {
        printf_warn("USB: failed to read device descriptor for slot %i", slot);
        return;
    }

    const auto* dev_desc = static_cast<usb_device_descriptor*>(dev_desc_buf);

    // iProduct/iManufacturer are just String Descriptor indices - fetch the actual strings
    const uint16_t lang_id = get_usb_string_lang_id(xhci, device);
    char product_name[USB_STRING_MAX_LEN];
    char manufacturer_name[USB_STRING_MAX_LEN];
    get_usb_string(xhci, device, dev_desc->product_idx, lang_id, product_name, sizeof(product_name));
    get_usb_string(xhci, device, dev_desc->manufacturer_idx, lang_id, manufacturer_name, sizeof(manufacturer_name));

    printf_info("USB: slot %i product=\"%s\" manufacturer=\"%s\" vendor=0x%x product_id=0x%x class=0x%x configs=%i",
                slot, product_name, manufacturer_name, dev_desc->vendor_id, dev_desc->product_id,
                dev_desc->device_class, dev_desc->num_configurations);

    // --- Configuration Descriptor (index 0): header first to learn total_length ---
    void* cfg_header_buf = Memory::physically_aligned_malloc(sizeof(usb_config_descriptor), 8, PAGE_SIZE);
    if (!cfg_header_buf)
    {
        printf_warn("USB: failed to allocate configuration descriptor header buffer for slot %i", slot);
        return;
    }

    const usb_device_request get_cfg_header_req = {
        .bm_request_type = 0x80,
        .b_request = USB_REQUEST_GET_DESCRIPTOR,
        .w_value = static_cast<uint16_t>(USB_DESCRIPTOR_TYPE_CONFIGURATION << 8),
        .w_index = 0,
        .w_length = sizeof(usb_config_descriptor)
    };

    actual = 0;
    if (!xhci->control_transfer(device, get_cfg_header_req, cfg_header_buf, &actual) || actual < sizeof(usb_config_descriptor))
    {
        printf_warn("USB: failed to read configuration descriptor header for slot %i", slot);
        return;
    }

    const uint16_t total_length = static_cast<usb_config_descriptor*>(cfg_header_buf)->total_length;

    // --- Full Configuration Descriptor: header + every Interface/Endpoint descriptor it owns ---
    void* cfg_buf = Memory::physically_aligned_malloc(total_length, 8, PAGE_SIZE);
    if (!cfg_buf)
    {
        printf_warn("USB: failed to allocate configuration descriptor buffer for slot %i", slot);
        return;
    }

    const usb_device_request get_cfg_req = {
        .bm_request_type = 0x80,
        .b_request = USB_REQUEST_GET_DESCRIPTOR,
        .w_value = static_cast<uint16_t>(USB_DESCRIPTOR_TYPE_CONFIGURATION << 8),
        .w_index = 0,
        .w_length = total_length
    };

    actual = 0;
    if (!xhci->control_transfer(device, get_cfg_req, cfg_buf, &actual) || actual < total_length)
    {
        printf_warn("USB: failed to read full configuration descriptor for slot %i", slot);
        return;
    }

    const uint8_t configuration_value = static_cast<usb_config_descriptor*>(cfg_buf)->configuration_value;

    // --- Walk the descriptor list looking for a Bulk-Only Transport Mass Storage interface ---
    bool in_bot_interface = false;
    uint8_t bot_interface_number = 0;
    uint8_t bulk_in_ep = 0, bulk_out_ep = 0;
    uint16_t bulk_in_mps = 0, bulk_out_mps = 0;

    const auto* p = static_cast<const uint8_t*>(cfg_buf);
    const uint8_t* end = p + total_length;

    while (p + 2 <= end && p[0] > 0)
    {
        const uint8_t desc_len = p[0];
        const uint8_t desc_type = p[1];

        if (desc_type == USB_DESCRIPTOR_TYPE_INTERFACE && desc_len >= sizeof(usb_interface_descriptor))
        {
            const auto* iface = reinterpret_cast<const usb_interface_descriptor*>(p);
            in_bot_interface = iface->interface_class == USB_CLASS_MASS_STORAGE &&
                                iface->interface_sub_class == USB_SUBCLASS_SCSI &&
                                iface->interface_protocol == USB_PROTOCOL_BULK_ONLY;
            if (in_bot_interface)
                bot_interface_number = iface->interface_number;
        }
        else if (desc_type == USB_DESCRIPTOR_TYPE_ENDPOINT && desc_len >= sizeof(usb_endpoint_descriptor_raw) && in_bot_interface)
        {
            const auto* ep = reinterpret_cast<const usb_endpoint_descriptor_raw*>(p);
            if ((ep->attributes & 0x3) == USB_ENDPOINT_TRANSFER_TYPE_BULK)
            {
                if (ep->endpoint_address & 0x80)
                {
                    bulk_in_ep = ep->endpoint_address;
                    bulk_in_mps = ep->max_packet_size;
                }
                else
                {
                    bulk_out_ep = ep->endpoint_address;
                    bulk_out_mps = ep->max_packet_size;
                }
            }
        }

        p += desc_len;
    }

    if (!bulk_in_ep || !bulk_out_ep)
    {
        printf_info("USB: slot %i has no Bulk-Only Transport Mass Storage interface, leaving unconfigured", slot);
        return;
    }

    // --- Tell the device to use this configuration (osdev enumeration step 8, part 1) ---
    const usb_device_request set_config_req = {
        .bm_request_type = 0x00, // Host-to-device | Standard | Device
        .b_request = USB_REQUEST_SET_CONFIGURATION,
        .w_value = configuration_value,
        .w_index = 0,
        .w_length = 0
    };

    if (!xhci->control_transfer(device, set_config_req))
    {
        printf_warn("USB: SET_CONFIGURATION failed for slot %i", slot);
        return;
    }

    // --- Bring the bulk endpoints up on the controller side (osdev enumeration step 8, part 2) ---
    vector<usb_endpoint_desc> endpoints;
    endpoints.push_back({
        .address = bulk_in_ep, .attributes = USB_ENDPOINT_TRANSFER_TYPE_BULK,
        .max_packet_size = bulk_in_mps, .xhci_interval = 0
    });
    endpoints.push_back({
        .address = bulk_out_ep, .attributes = USB_ENDPOINT_TRANSFER_TYPE_BULK,
        .max_packet_size = bulk_out_mps, .xhci_interval = 0
    });

    if (!xhci->configure_endpoints(device, endpoints))
    {
        printf_warn("USB: failed to configure bulk endpoints for slot %i", slot);
        return;
    }

    usb_mass_storage_device msd{};
    msd.device = device;
    msd.interface_number = bot_interface_number;
    msd.bulk_in_endpoint = bulk_in_ep;
    msd.bulk_out_endpoint = bulk_out_ep;
    msd.bulk_in_max_packet_size = bulk_in_mps;
    msd.bulk_out_max_packet_size = bulk_out_mps;
    memcpy(msd.product_name, product_name, sizeof(msd.product_name));
    memcpy(msd.manufacturer_name, manufacturer_name, sizeof(msd.manufacturer_name));
    mass_storage_devices.push_back(msd);

    printf_info("USB: mass storage device \"%s\" ready on slot %i (bulk in=0x%x out=0x%x)",
                product_name, slot, bulk_in_ep, bulk_out_ep);
}
