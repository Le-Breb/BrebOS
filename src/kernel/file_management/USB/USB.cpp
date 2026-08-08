#include "USB.h"

#include "../../core/memory/memory.h"

__attribute__ ((format (printf, 1, 2))) extern int printf_warn(const char* format, ...);
__attribute__ ((format (printf, 1, 2))) extern int printf_info(const char* format, ...);

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
    printf_info("USB: slot %i vendor=0x%x product=0x%x class=0x%x configs=%i",
                slot, dev_desc->vendor_id, dev_desc->product_id, dev_desc->device_class,
                dev_desc->num_configurations);

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

    mass_storage_devices.push_back({
        .device = device,
        .bulk_in_endpoint = bulk_in_ep,
        .bulk_out_endpoint = bulk_out_ep,
        .bulk_in_max_packet_size = bulk_in_mps,
        .bulk_out_max_packet_size = bulk_out_mps
    });

    printf_info("USB: mass storage device ready on slot %i (bulk in=0x%x out=0x%x)", slot, bulk_in_ep, bulk_out_ep);
}
