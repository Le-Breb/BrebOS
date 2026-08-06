#include "xHCI.h"

#include "xHCI_common.h"
#include "xHCI_extension_capabilities.h"
#include "xHCI_mem.h"
#include "../../core/PIT.h"
#include "../../core/memory/memory.h"
#include "../../processes/process.h"

[[noreturn]]
extern int irrecoverable_error(const char* format, ...);

__attribute__ ((format (printf, 1, 2)))
extern int printf_info(const char* format, ...);

xHCI::xHCI(const Device& device) : Device(device)
{
    const uint32_t bar0 = PCI::getPCIBar(device.bus, device.device, device.function, 0) & ~1;
    const uint32_t bar1 = PCI::getPCIBar(device.bus, device.device, device.function, 1) & ~3;

    // addr is ((static_cast<uint64_t>(bar1) << 32) | (bar0 & ~0xFULL));
    if (bar1)
        irrecoverable_error("xCHI bar1 is non zero, MMIO is not addressable (> 32bits)");

    const uint32_t bar_size = PCI::getPCIBarSize(device.bus, device.device, device.function, 0);

    mmio = reinterpret_cast<void*>(bar0 & ~0xFULL);
    if (!Memory::identity_map((uint32_t)mmio, bar_size))
        irrecoverable_error("Couldn't identity map xHCI controller MMIO");

    parse_capability_registers();
    parse_extended_capabilities();
    if (!reset_controller())
        irrecoverable_error("xHCI controller reset failed");
    configure_operational_registers();
    configure_runtime_registers();

    if (!Interrupts::register_interrupt(
            PIC1_START_INTERRUPT + PCI::getIntLine(device.bus, device.device, device.function),
            this
        ))
        irrecoverable_error("xHCI controller interrupt registration failed");
}

bool xHCI::reset_controller() const
{
    // Make sure we clear the Run/Stop bit
    uint32_t usbcmd = op_regs->usbcmd;
    usbcmd &= ~XHCI_USBCMD_RUN_STOP;
    op_regs->usbcmd = usbcmd;

    // Wait for the HCHalted bit to be set
    uint32_t timeout = 200;
    while (!(op_regs->usbsts & XHCI_USBSTS_HCH)) {
        if (--timeout == 0) {
            printf_warn("XHCI host controller did not halt within %ums\n", timeout);
            return false;
        }

        PIT::sleep(1);
    }

    // Set the HC Reset bit
    usbcmd = op_regs->usbcmd;
    usbcmd |= XHCI_USBCMD_HCRESET;
    op_regs->usbcmd = usbcmd;

    // Wait for this bit and CNR bit to clear
    timeout = 1000;
    while (
        op_regs->usbcmd & XHCI_USBCMD_HCRESET ||
        op_regs->usbsts & XHCI_USBSTS_CNR
    ) {
        if (--timeout == 0) {
            printf_warn("Host controller did not reset within %ums\n", timeout);
            return false;
        }

        PIT::sleep(1);
    }

    PIT::sleep(50);

    // Check the defaults of the operational registers
    if (op_regs->usbcmd != 0)
        return false;

    if (op_regs->dnctrl != 0)
        return false;

    if (op_regs->crcr != 0)
        return false;

    if (op_regs->dcbaap)
        return false;

    if (op_regs->config != 0)
        return false;

    return true;
}

void xHCI::configure_operational_registers()
{
    // Enable device notifications
    op_regs->dnctrl = 0xffff;

    // Configure the usbconfig field
    op_regs->config = XHCI_MAX_DEVICE_SLOTS(cap_regs);

    // Setup device context base address array and scratchpad buffers
    setup_dcbaa();

    // Setup the command ring and write CRCR
    command_ring = {new xhci_command_ring(XHCI_COMMAND_RING_TRB_COUNT)};
    op_regs->crcr = command_ring->get_physical_base() | command_ring->get_cycle_bit();
}

void xHCI::setup_dcbaa()
{
    size_t dcbaa_size = sizeof(uintptr_t) * (XHCI_MAX_DEVICE_SLOTS(cap_regs) + 1);

    if (void* dcbaa_addr = Memory::physically_aligned_malloc(dcbaa_size, XHCI_DEVICE_CONTEXT_ALIGNMENT, XHCI_DEVICE_CONTEXT_BOUNDARY); dcbaa_addr)
        dcbaa = static_cast<uint64_t*>(dcbaa_addr);
    else
        irrecoverable_error("%s: physically_aligned_malloc failed", __func__);

    const auto max_scratchpad_buffers = XHCI_MAX_SCRATCHPAD_BUFFERS(cap_regs);

    /*
    // xHci Spec Section 6.1 (page 404)

    If the Max Scratchpad Buffers field of the HCSPARAMS2 register is > ‘0’, then
    the first entry (entry_0) in the DCBAA shall contain a pointer to the Scratchpad
    Buffer Array. If the Max Scratchpad Buffers field of the HCSPARAMS2 register is
    = ‘0’, then the first entry (entry_0) in the DCBAA is reserved and shall be
    cleared to ‘0’ by software.
    */

    // Initialize scratchpad buffer array if needed
    if (XHCI_MAX_SCRATCHPAD_BUFFERS(cap_regs) > 0) {
        uint64_t* scratchpad_array = static_cast<uint64_t*>(
            Memory::physically_aligned_malloc(
                max_scratchpad_buffers * sizeof(uint64_t),
                XHCI_DEVICE_CONTEXT_ALIGNMENT,
                XHCI_DEVICE_CONTEXT_BOUNDARY
            )
        );

        // Create scratchpad pages
        for (uint8_t i = 0; i < max_scratchpad_buffers; i++) {
            void* scratchpad = Memory::physically_aligned_malloc(
                PAGE_SIZE,
                XHCI_SCRATCHPAD_BUFFERS_ALIGNMENT,
                XHCI_SCRATCHPAD_BUFFERS_BOUNDARY
            );

            scratchpad_array[i] = PHYS_ADDR(Memory::page_tables, (uintptr_t)scratchpad);
        }

        // Set the first slot in the DCBAA to point to the scratchpad array
        dcbaa[0] = PHYS_ADDR(Memory::page_tables, (uintptr_t)scratchpad_array);
    }

    // Set DCBAA pointer in the operational registers
    op_regs->dcbaap = reinterpret_cast<uintptr_t>(dcbaa);
}

void xHCI::parse_capability_registers()
{
    cap_regs = static_cast<volatile xhci_capability_registers*>(mmio);

    m_capability_regs_length = cap_regs->caplength;

    m_max_device_slots = XHCI_MAX_DEVICE_SLOTS(cap_regs);
    m_max_interrupters = XHCI_MAX_INTERRUPTERS(cap_regs);
    m_max_ports = XHCI_MAX_PORTS(cap_regs);

    m_isochronous_scheduling_threshold = XHCI_IST(cap_regs);
    m_erst_max = XHCI_ERST_MAX(cap_regs);
    m_max_scratchpad_buffers = XHCI_MAX_SCRATCHPAD_BUFFERS(cap_regs);

    m_64bit_addressing_capability = XHCI_AC64(cap_regs);
    m_bandwidth_negotiation_capability = XHCI_BNC(cap_regs);
    m_64byte_context_size = XHCI_CSZ(cap_regs);
    m_port_power_control = XHCI_PPC(cap_regs);
    m_port_indicators = XHCI_PIND(cap_regs);
    m_light_reset_capability = XHCI_LHRC(cap_regs);
    m_extended_capabilities_offset = XHCI_XECP(cap_regs) * sizeof(uint32_t);

    // Update the base pointer to operational register set
    op_regs = reinterpret_cast<volatile xhci_operational_registers*>(static_cast<char*>(mmio) + cap_regs->caplength);

    // Update the base pointer to the runtime register set
    runtime_regs = reinterpret_cast<volatile xhci_runtime_registers*>(static_cast<char*>(mmio) + cap_regs->rtsoff);

    // Construct a manager class instance for the doorbell register array
    doorbell_manager = {new xhci_doorbell_manager(reinterpret_cast<uintptr_t>(mmio) + cap_regs->dboff)};
}

void xHCI::parse_extended_capabilities()
{
    volatile uint32_t* head_cap_ptr = reinterpret_cast<volatile uint32_t*>(
        reinterpret_cast<uintptr_t>(mmio) + m_extended_capabilities_offset
    );

    extended_capabilities_head = { new xhci_extended_capability(head_cap_ptr) };

    auto node = extended_capabilities_head;
    while (node.get())
    {
        if (node->id() == xhci_extended_capability_code::supported_protocol) {
            xhci_usb_supported_protocol_capability cap(node->base());

            if (cap.compatible_port_count > 0)
            {
                // Make the ports zero-based
                uint8_t first_port = cap.compatible_port_offset - 1;
                uint8_t last_port = first_port + cap.compatible_port_count - 1;

                if (cap.major_revision_version == 3)
                    for (uint8_t port = first_port; port <= last_port; port++)
                        usb3_ports.push_back(port);
            }
        }

        // Advance to the next node
        node = node->next();
    }
}

void xHCI::configure_runtime_registers()
{
    // Get the primary interrupter registers
    volatile xhci_interrupter_registers* interrupter_regs = &runtime_regs->ir[0];

    // Enable interrupts
    uint32_t iman = interrupter_regs->iman;
    iman |= XHCI_IMAN_INTERRUPT_ENABLE;
    interrupter_regs->iman = iman;

    // Setup the event ring and write to interrupter
    // registers to set ERSTSZ, ERSDP, and ERSTBA.
    event_ring = {new xhci_event_ring(XHCI_EVENT_RING_TRB_COUNT, interrupter_regs)};

    // Clear any pending interrupts for primary interrupter
    acknowledge_irq(0);
}

void xHCI::acknowledge_irq(uint8_t interrupter) const
{
    // Clear the EINT bit in USBSTS by writing '1' to it
    op_regs->usbsts = XHCI_USBSTS_EINT;

    // Get the interrupter registers
    volatile xhci_interrupter_registers* interrupter_regs = &runtime_regs->ir[interrupter];

    // Read the current value of IMAN
    uint32_t iman = interrupter_regs->iman;

    // Set the IP bit to '1' to clear it, preserve other bits including IE
    iman |= XHCI_IMAN_INTERRUPT_PENDING;

    // Write back to IMAN
    interrupter_regs->iman = iman;
}

bool xHCI::start_host_controller() const
{
    // Ensure USBCMD bits for RUN/STOP are properly set
    uint32_t usbcmd = op_regs->usbcmd;
    usbcmd |= XHCI_USBCMD_RUN_STOP;
    usbcmd |= XHCI_USBCMD_INTERRUPTER_ENABLE;
    usbcmd |= XHCI_USBCMD_HOSTSYS_ERROR_ENABLE;
    op_regs->usbcmd = usbcmd;

    // Ensure the controller transitions out of the halted state
    constexpr int max_retries = 1000;
    int retries = 0;

    while (op_regs->usbsts & XHCI_USBSTS_HCH) {
        if (retries++ >= max_retries) {
            // Timeout: Controller failed to start
            return false;
        }
        PIT::sleep(1); // Poll every 1 ms for responsiveness
    }

    // Verify CNR (Controller Not Ready) bit is clear
    if (op_regs->usbsts & XHCI_USBSTS_CNR) {
        return false; // Controller is not ready
    }

    // Controller started successfully
    return true;
}

void xHCI::process_events()
{
    // Poll the event ring for any events
    vector<xhci_trb_t*> events;
    if (event_ring->has_unprocessed_events())
        event_ring->dequeue_events(events);

    uint8_t command_completion_status = 0;

    for (const auto event : events)
    {
        switch (event->trb_type)
        {
            case XHCI_TRB_TYPE_CMD_COMPLETION_EVENT:
                command_completion_status = 1;
                command_completion_events.push_back((xhci_command_completion_trb_t*)event);
                break;
            case XHCI_TRB_TYPE_PORT_STATUS_CHANGE_EVENT:
            {
                const auto* psc_event = (xhci_port_status_change_trb_t*)event;
                // Port IDs in the event are 1-based; read_portsc_reg()/reset_port() expect a 0-based index
                handle_port_connect_change(psc_event->port_id - 1);
                break;
            }
            default:
                printf_info("Unhandled event TRB type: %u. Status: 0x%x", event->trb_type, event->status);
                break;
        }
    }

    command_irq_completed = command_completion_status;
}

bool xHCI::is_usb3_port(uint8_t port) const
{
    for (const auto usb2_port : usb3_ports)
        if (usb2_port == port)
            return true;

    return false;
}

xhci_portsc_register xHCI::read_portsc_reg(uint8_t port_num) const
{
    uint64_t reg_base = reinterpret_cast<uint64_t>(op_regs) + (0x400 + (0x10 * port_num));

    xhci_portsc_register reg;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(reg_base);

    return reg;
}

void xHCI::write_portsc_reg(xhci_portsc_register reg, uint8_t port_num)
{
    uint64_t reg_base = reinterpret_cast<uint64_t>(op_regs) + (0x400 + (0x10 * port_num));
    *reinterpret_cast<volatile uint32_t*>(reg_base) = reg.raw;
}

bool xHCI::reset_port(uint8_t port_num)
{
    xhci_portsc_register portsc = read_portsc_reg(port_num);

    const bool is_usb3 = is_usb3_port(port_num);

    // Power on the port if necessary
    if (portsc.pp == 0) {
        portsc.pp = 1;
        write_portsc_reg(portsc, port_num);
        PIT::sleep(20); // Wait for power stabilization
        portsc = read_portsc_reg(port_num);

        if (portsc.pp == 0) {
            printf_warn("%s: Port %i: Failed to power on port", __PRETTY_FUNCTION__, port_num);
            return false;
        }
    }

    // Clear any lingering status change bits before initiating the reset
    portsc.csc = 1; // Clear connect status change
    portsc.pec = 1; // Clear port enable/disable change
    portsc.prc = 1; // Clear port reset change
    write_portsc_reg(portsc, port_num);

    // Initiate the port reset
    if (is_usb3) {
        portsc.wpr = 1; // Warm reset for USB 3.0
    } else {
        portsc.pr = 1; // Standard port reset for USB 2.0
    }
    write_portsc_reg(portsc, port_num);

    // Wait for the reset to complete
    int timeout = 100;
    while (timeout > 0) {
        portsc = read_portsc_reg(port_num);

        if ((is_usb3 && portsc.wrc) || (!is_usb3 && portsc.prc)) {
            break; // Reset has completed
        }

        timeout--;
        PIT::sleep(1);
    }

    if (timeout == 0) {
        printf_warn("%s: Port %i: Port reset timed out", __PRETTY_FUNCTION__ , port_num);
        return false;
    }

    PIT::sleep(3); // Give the hardware time to settle

    // Clear the reset completion and status change bits
    portsc.prc = 1; // Clear port reset change
    portsc.wrc = 1; // Clear warm reset change (USB 3.0)
    portsc.csc = 1; // Clear connect status change
    portsc.pec = 1; // Clear port enable/disable change
    portsc.ped = 0; // Don't clear the PED bit
    write_portsc_reg(portsc, port_num);

    PIT::sleep(3);

    // Re-read the register to check if the port is enabled
    portsc = read_portsc_reg(port_num);

    // This case could happen when the port has been reset after
    // a device disconnect event, and no device has connected since.
    if (portsc.ped == 0) {
        return false;
    }

    return true;
}

xhci_command_completion_trb_t* xHCI::send_command_trb(xhci_trb_t* cmd_trb, uint32_t timeout_ms)
{
    // Enqueue the TRB
    command_ring->enqueue(cmd_trb);

    // Ring the command doorbell
    doorbell_manager->ring_command_doorbell();

    // Wait for the IRQ and let the host controller process the command
    uint64_t sleep_passed = 0;
    while (!command_irq_completed) {
        PIT::spin_sleep(10);
        sleep_passed += 10;

        if (sleep_passed > timeout_ms * 1000) {
            break;
        }
    }

    // ** Important Assumption **
    //  - Only one command is being sent to the controller at a time
    xhci_command_completion_trb_t* completion_trb =
        command_completion_events.get_size() ? command_completion_events[0] : nullptr;

    // Reset the irq flag and clear out the command completion event queue
    command_completion_events.clear();
    command_irq_completed = 0;

    if (!completion_trb)
        irrecoverable_error("Failed to find completion TRB for command %i\n", cmd_trb->trb_type);

    if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS)
        irrecoverable_error("Command TRB failed with error: %s\n", trb_completion_code_to_string(completion_trb->completion_code));

    return completion_trb;
}

const char* xHCI::_usb_speed_to_string(uint8_t speed)
{
    static const char* speed_string[7] = {
        "Invalid",
        "Full Speed (12 MB/s - USB2.0)",
        "Low Speed (1.5 Mb/s - USB 2.0)",
        "High Speed (480 Mb/s - USB 2.0)",
        "Super Speed (5 Gb/s - USB3.0)",
        "Super Speed Plus (10 Gb/s - USB 3.1)",
        "Undefined"
    };

    return speed_string[speed];
}

xHCI* xHCI::get_instance()
{
    static xHCI* instance = nullptr;

    if (instance)
        return instance;
    if (PCI::xCHI.bus == (uint8_t)-1u)
        return nullptr;
    return instance = new xHCI(PCI::xCHI);
}

void xHCI::start()
{
    if (!start_host_controller())
        irrecoverable_error("xHCI controller failed to start");

    printf_info("xHCI controller started successfully");

    // for (uint8_t port = 0; port < m_max_ports; port++)
    //     printf_info("Port %u: %s", port + 1, is_usb3_port(port) ? "USB2" : "USB3");

    for (uint8_t port = 0; port < m_max_ports; port++)
        handle_port_connect_change(port);
}

void xHCI::handle_port_connect_change(uint8_t port_num)
{
    const xhci_portsc_register portsc = read_portsc_reg(port_num);

    if (portsc.csc && portsc.ccs)
    {
        if (reset_port(port_num))
            printf_info("Device connected on port %i - %s", port_num, _usb_speed_to_string(portsc.port_speed));
        else
            printf_warn("Failed to reset port %i after device connection", port_num);
    }
}

void xHCI::fire([[maybe_unused]] cpu_state_t* cpu_state, [[maybe_unused]] stack_state_t* stack_state)
{
    process_events();
    acknowledge_irq(0);
}
