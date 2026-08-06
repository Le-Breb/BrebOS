#ifndef BREBOS_XHCI_H
#define BREBOS_XHCI_H

// Most of xHCI code comes from https://github.com/FlareCoding/stellux-xhci-tutorial/

#include "xHCI_registers.h"
#include "xHCI_rings.h"
#include "../../core/interrupt_handler.h"
#include "../../core/PCI.h"
#include "../../utils/shared_pointer.h"

class xHCI : public PCI::Device, public Interrupt_handler
{
    void* mmio = nullptr;
    uint64_t* dcbaa = nullptr;

    SharedPointer<xhci_command_ring> command_ring;
    SharedPointer<xhci_event_ring> event_ring;
    SharedPointer<xhci_doorbell_manager> doorbell_manager;
    vector<xhci_command_completion_trb_t*> command_completion_events;
    volatile uint8_t command_irq_completed = 0;

    SharedPointer<xhci_extended_capability> extended_capabilities_head;

    vector<uint8_t> usb3_ports;

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

    static const char* _usb_speed_to_string(uint8_t speed);

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
};


#endif //BREBOS_XHCI_H
