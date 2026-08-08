#ifndef BREBOS_USB_H
#define BREBOS_USB_H

#include "xHCI.h"
#include "USB_common.h"

// A USB device found to expose a Bulk-Only Transport Mass Storage interface, fully configured
// (SET_CONFIGURATION issued, bulk endpoints brought up via xHCI::configure_endpoints()) and
// ready for the caller to drive CBW/CSW/SCSI traffic over via xHCI::bulk_transfer().
struct usb_mass_storage_device
{
    SharedPointer<xhci_device> device;
    uint8_t  bulk_in_endpoint;         // bEndpointAddress, bit 7 set (IN)
    uint8_t  bulk_out_endpoint;        // bEndpointAddress, bit 7 clear (OUT)
    uint16_t bulk_in_max_packet_size;
    uint16_t bulk_out_max_packet_size;
};

class USB
{
public:
    [[nodiscard]] static USB* get_instance();

    // Runs full enumeration (Device + Configuration descriptors, interface/endpoint parsing,
    // SET_CONFIGURATION, endpoint configuration) for every xHCI device that hasn't been
    // processed yet. Devices exposing a Bulk-Only Transport Mass Storage interface end up in
    // get_mass_storage_devices(), ready for BOT/SCSI command traffic. Safe to call again after
    // new devices show up (e.g. hotplug) - already-processed slots are skipped.
    void enumerate_devices();

    [[nodiscard]] const vector<usb_mass_storage_device>& get_mass_storage_devices() const
    {
        return mass_storage_devices;
    }

private:
    vector<uint8_t> processed_slots;
    vector<usb_mass_storage_device> mass_storage_devices;

    void enumerate_device(const SharedPointer<xhci_device>& device);
};


#endif //BREBOS_USB_H
