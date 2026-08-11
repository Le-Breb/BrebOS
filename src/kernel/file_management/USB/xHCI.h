#ifndef BREBOS_XHCI_H
#define BREBOS_XHCI_H

// Most of xHCI code comes from https://github.com/FlareCoding/stellux-xhci-tutorial/

#include "xHCI_registers.h"
#include "xHCI_rings.h"
#include "xHCI_device.h"
#include "USB_common.h"
#include "../../core/interrupt_handler.h"
#include "../../core/PCI.h"
#include "../../utils/shared_pointer.h"
#include "../../utils/Status.h"

class xHCI : public PCI::Device, public Interrupt_handler
{
    void* mmio = nullptr;
    uint64_t* dcbaa = nullptr;

    SharedPointer<xhci_command_ring> command_ring;
    SharedPointer<xhci_event_ring> event_ring;
    SharedPointer<xhci_doorbell_manager> doorbell_manager;
    vector<xhci_command_completion_trb_t*> command_completion_events;
    volatile uint8_t command_irq_completed = 0;

    vector<xhci_transfer_event_trb_t*> transfer_completion_events;
    volatile uint8_t transfer_irq_completed = 0;

    SharedPointer<xhci_extended_capability> extended_capabilities_head;

    vector<uint8_t> usb3_ports;

    vector<SharedPointer<xhci_device>> devices;

    explicit xHCI(const Device& device);
    bool reset_controller() const;
    void configure_operational_registers();
    void setup_dcbaa();
    void parse_capability_registers();
    void parse_extended_capabilities();
    void configure_runtime_registers();
    void acknowledge_irq(uint8_t interrupter) const;
    bool start_host_controller() const;
    void process_events();
    bool is_usb3_port(uint8_t port) const;
    xhci_portsc_register read_portsc_reg(uint8_t port_num) const;
    void write_portsc_reg(xhci_portsc_register reg, uint8_t port_num);
    bool reset_port(uint8_t port_num);
    void handle_port_connect_change(uint8_t port_num);
    xhci_command_completion_trb_t* send_command_trb(xhci_trb_t* cmd_trb, uint32_t timeout_ms = 200);
    xhci_transfer_event_trb_t* wait_for_transfer_event(uint32_t timeout_ms = 500);

    static const char* _usb_speed_to_string(uint8_t speed);
    static uint16_t default_control_max_packet_size(uint8_t speed);
    static uint8_t endpoint_dci(uint8_t endpoint_number, bool is_in);

    // Enumeration bring-up: Enable Slot -> Address Device -> read the first 8 bytes of the
    // Device Descriptor to learn the real bMaxPacketSize0 (only variable for Full-Speed
    // devices). Leaves the device with a working default control pipe, ready for the caller
    // to drive the rest of enumeration (full descriptors, set configuration, ...) itself via
    // control_transfer()/configure_endpoints()/bulk_transfer() below.
    void setup_device(uint8_t port_num);
    uint8_t enable_device_slot();
    bool create_device_context(uint8_t slot_id);
    bool address_device(const SharedPointer<xhci_device>& device);
    bool evaluate_context(const SharedPointer<xhci_device>& device);

    struct xhci_capability_registers
    {
        uint8_t caplength;
        uint8_t reserved;
        uint16_t hciversion;
        uint32_t hcsparams1;
        uint32_t hcsparams2;
        uint32_t hcsparams3;
        uint32_t hccparams1;
        uint32_t dboff;
        uint32_t rtsoff;
    } __attribute__((packed));

    volatile xhci_capability_registers* cap_regs = nullptr;
    volatile xhci_operational_registers* op_regs = nullptr;
    volatile xhci_runtime_registers* runtime_regs = nullptr;

    // CAPLENGTH
    uint8_t m_capability_regs_length;

    // HCSPARAMS1
    uint8_t m_max_device_slots;
    uint8_t m_max_interrupters;
    uint8_t m_max_ports;

    // HCSPARAMS2
    uint8_t m_isochronous_scheduling_threshold;
    uint8_t m_erst_max;
    uint8_t m_max_scratchpad_buffers;

    // hccparams1
    bool m_64bit_addressing_capability;
    bool m_bandwidth_negotiation_capability;
    bool m_64byte_context_size;
    bool m_port_power_control;
    bool m_port_indicators;
    bool m_light_reset_capability;
    uint32_t m_extended_capabilities_offset;
public:
    [[nodiscard]] static xHCI* get_instance();

    void start();
    void fire(cpu_state_t* cpu_state, stack_state_t* stack_state) override;

    // Devices that completed Address Device (and are sitting on a working default control
    // pipe). Populated automatically as ports connect.
    [[nodiscard]] const vector<SharedPointer<xhci_device>>& get_devices() const { return devices; }

    // Runs a full control transfer (Setup [+ Data] [+ Status] stages) on device's default
    // control endpoint. `data` must point to physically-contiguous, identity-mapped memory
    // (e.g. from Memory::physically_aligned_malloc) large enough for request.w_length bytes;
    // pass nullptr when w_length is 0. actual_length, if given, receives the number of bytes
    // actually transferred during the data stage (may be less than w_length on a short packet).
    bool control_transfer(const SharedPointer<xhci_device>& device, const usb_device_request& request,
                           void* data = nullptr, uint32_t* actual_length = nullptr);

    // Issues a Configure Endpoint Command adding (or updating) the given endpoints, allocating
    // a Transfer Ring for each one that doesn't already have one. Caller supplies the endpoint
    // descriptor fields as read from a Configuration Descriptor.
    bool configure_endpoints(const SharedPointer<xhci_device>& device, const vector<usb_endpoint_desc>& endpoints);

    // Runs a bulk transfer on a previously-configured (non-control) endpoint. `data` must be
    // physically-contiguous, identity-mapped memory. Transparently splits `length` into
    // multiple chained Normal TRBs if needed. actual_length, if given, receives the number of
    // bytes actually transferred (may be less than length on a short packet).
    Status bulk_transfer(const SharedPointer<xhci_device>& device, uint8_t endpoint_address,
                         void* data, uint32_t length, uint32_t* actual_length = nullptr);
};


#endif //BREBOS_XHCI_H
