#include "xHCI.h"

#include "xHCI_common.h"
#include "xHCI_extension_capabilities.h"
#include "xHCI_mem.h"
#include "../../core/PIT.h"
#include "../../core/memory/memory.h"
#include "../../processes/process.h"
#include <kstring.h>
#include "../../core/fb.h"
#include "../../processes/scheduler.h"

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

            scratchpad_array[i] = PHYS_ADDR(Scheduler::get_current_page_tables(), (uintptr_t)scratchpad);
        }

        // Set the first slot in the DCBAA to point to the scratchpad array
        dcbaa[0] = PHYS_ADDR(Scheduler::get_current_page_tables(), (uintptr_t)scratchpad_array);
    }

    // Set DCBAA pointer in the operational registers
    op_regs->dcbaap = PHYS_ADDR(Scheduler::get_current_page_tables(), (uintptr_t)dcbaa);
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
    uint8_t transfer_completion_status = 0;

    for (const auto event : events)
    {
        switch (event->trb_type)
        {
            case XHCI_TRB_TYPE_CMD_COMPLETION_EVENT:
                command_completion_status = 1;
                command_completion_events.push_back((xhci_command_completion_trb_t*)event);
                break;
            case XHCI_TRB_TYPE_TRANSFER_EVENT:
                transfer_completion_status = 1;
                transfer_completion_events.push_back((xhci_transfer_event_trb_t*)event);
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
    transfer_irq_completed = transfer_completion_status;
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

xhci_transfer_event_trb_t* xHCI::wait_for_transfer_event(uint32_t timeout_ms)
{
    // ** Important Assumption **
    //  - Only one transfer is in flight at a time, same assumption as send_command_trb()
    uint64_t sleep_passed = 0;
    while (!transfer_irq_completed) {
        PIT::spin_sleep(10);
        sleep_passed += 10;

        if (sleep_passed > timeout_ms * 1000) {
            break;
        }
    }

    xhci_transfer_event_trb_t* event =
        transfer_completion_events.get_size() ? transfer_completion_events[0] : nullptr;

    transfer_completion_events.clear();
    transfer_irq_completed = 0;

    return event;
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

uint16_t xHCI::default_control_max_packet_size(uint8_t speed)
{
    // xHci Spec Section 4.3 / USB2 Spec Table 5-5: default control pipe max packet size is fixed
    // by speed, except for Full-Speed devices where it can be 8, 16, 32 or 64 and must be read
    // from the device's Device Descriptor (see setup_device()).
    switch (speed)
    {
        case XHCI_USB_SPEED_LOW_SPEED: return 8;
        case XHCI_USB_SPEED_FULL_SPEED: return 8; // conservative guess, corrected after reading the device descriptor
        case XHCI_USB_SPEED_HIGH_SPEED: return 64;
        case XHCI_USB_SPEED_SUPER_SPEED:
        case XHCI_USB_SPEED_SUPER_SPEED_PLUS: return 512;
        default: return 8;
    }
}

uint8_t xHCI::endpoint_dci(uint8_t endpoint_number, bool is_in)
{
    // xHci Spec Section 4.5.1: Device Context Index (DCI) = (Endpoint Number * 2) + Direction.
    // endpoint_number is 1-based (endpoint 0/the control endpoint always uses DCI 1 directly).
    return static_cast<uint8_t>(2 * endpoint_number + (is_in ? 1 : 0));
}

uint8_t xHCI::enable_device_slot()
{
    xhci_trb_t trb{};
    trb.trb_type = XHCI_TRB_TYPE_ENABLE_SLOT_CMD;

    return send_command_trb(&trb)->slot_id;
}

bool xHCI::create_device_context(uint8_t slot_id)
{
    const size_t device_context_size = m_64byte_context_size ? sizeof(xhci_device_context64) : sizeof(xhci_device_context32);

    void* ctx = Memory::physically_aligned_malloc(
        device_context_size,
        XHCI_DEVICE_CONTEXT_ALIGNMENT,
        XHCI_DEVICE_CONTEXT_BOUNDARY
    );
    if (!ctx)
        return false;

    memset(ctx, 0, device_context_size);

    // Ownership of the Output Device Context passes to the xHC once the doorbell is rung for
    // the first Address Device Command targeting this slot (xHci Spec Section 6.2.1)
    dcbaa[slot_id] = PHYS_ADDR(Scheduler::get_current_page_tables(), (uintptr_t)ctx);

    return true;
}

bool xHCI::address_device(const SharedPointer<xhci_device>& device)
{
    if (!device->has_transfer_ring(1))
        device->create_transfer_ring(1, XHCI_TRANSFER_RING_TRB_COUNT);

    xhci_input_control_context32* input_ctrl_ctx = device->get_input_control_ctx();
    input_ctrl_ctx->add_flags = (1u << 0) | (1u << 1); // A0: slot context, A1: control endpoint (DCI 1)

    xhci_slot_context32* slot_ctx = device->get_input_slot_ctx();
    slot_ctx->route_string = 0; // Device is attached directly to a root hub port, no hubs involved
    slot_ctx->speed = device->get_speed();
    slot_ctx->context_entries = 1; // Only the control endpoint is valid so far
    slot_ctx->root_hub_port_num = device->get_port();

    xhci_endpoint_context32* ep0_ctx = device->get_input_control_ep_ctx();
    ep0_ctx->endpoint_type = XHCI_ENDPOINT_TYPE_CONTROL;
    ep0_ctx->max_packet_size = default_control_max_packet_size(device->get_speed());
    ep0_ctx->error_count = 3;
    ep0_ctx->average_trb_length = 8; // Setup Stage TRBs are always 8 bytes; refined once real traffic flows
    ep0_ctx->transfer_ring_dequeue_ptr = (device->get_transfer_ring(1)->get_physical_base() & ~0xFULL) | 1 /* DCS */;

    xhci_trb_t trb{};
    trb.trb_type = XHCI_TRB_TYPE_ADDRESS_DEVICE_CMD;
    trb.parameter = device->get_input_ctx_physical_addr();
    trb.control |= static_cast<uint32_t>(device->get_slot()) << XHCI_TRB_CMD_SLOT_ID_SHIFT;

    return send_command_trb(&trb) != nullptr;
}

bool xHCI::evaluate_context(const SharedPointer<xhci_device>& device)
{
    xhci_input_control_context32* input_ctrl_ctx = device->get_input_control_ctx();
    input_ctrl_ctx->drop_flags = 0;
    input_ctrl_ctx->add_flags = 1u << 1; // A1: control endpoint context only

    xhci_trb_t trb{};
    trb.trb_type = XHCI_TRB_TYPE_EVALUATE_CONTEXT_CMD;
    trb.parameter = device->get_input_ctx_physical_addr();
    trb.control |= static_cast<uint32_t>(device->get_slot()) << XHCI_TRB_CMD_SLOT_ID_SHIFT;

    return send_command_trb(&trb) != nullptr;
}

bool xHCI::control_transfer(const SharedPointer<xhci_device>& device, const usb_device_request& request,
                             void* data, uint32_t* actual_length)
{
    if (actual_length)
        *actual_length = 0;

    const SharedPointer<xhci_transfer_ring>& ring = device->get_transfer_ring(1);
    if (!ring)
        return false;

    const bool has_data = request.w_length > 0;
    const bool device_to_host = (request.bm_request_type & 0x80) != 0;

    // Setup Stage TRB - the 8-byte SETUP packet is placed directly in "parameter" (Immediate Data)
    xhci_trb_t setup_trb{};
    memcpy(&setup_trb.parameter, &request, sizeof(usb_device_request));
    setup_trb.status = sizeof(usb_device_request);
    setup_trb.trb_type = XHCI_TRB_TYPE_SETUP_STAGE;
    setup_trb.immediate_data = 1;
    setup_trb.control |= (has_data ? (device_to_host ? XHCI_TRB_TRT_IN_DATA_STAGE : XHCI_TRB_TRT_OUT_DATA_STAGE)
                                    : XHCI_TRB_TRT_NO_DATA_STAGE) << XHCI_TRB_TRT_SHIFT;
    ring->enqueue(&setup_trb);

    if (has_data)
    {
        xhci_trb_t data_trb{};
        data_trb.parameter = PHYS_ADDR(Scheduler::get_current_page_tables(), (uintptr_t)data);
        data_trb.status = request.w_length;
        data_trb.trb_type = XHCI_TRB_TYPE_DATA_STAGE;
        if (device_to_host)
            data_trb.control |= XHCI_TRB_DIR_IN_BIT;
        ring->enqueue(&data_trb);
    }

    // Status stage direction is always opposite the data stage (IN when there was no data stage)
    const bool status_dir_in = !(has_data && device_to_host);

    xhci_trb_t status_trb{};
    status_trb.trb_type = XHCI_TRB_TYPE_STATUS_STAGE;
    status_trb.interrupt_on_completion = 1;
    if (status_dir_in)
        status_trb.control |= XHCI_TRB_DIR_IN_BIT;
    ring->enqueue(&status_trb);

    doorbell_manager->ring_control_endpoint_doorbell(device->get_slot());

    const xhci_transfer_event_trb_t* event = wait_for_transfer_event();
    if (!event)
        return false;

    if (event->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS &&
        event->completion_code != XHCI_TRB_COMPLETION_CODE_SHORT_PACKET)
        return false;

    if (actual_length)
        *actual_length = request.w_length - event->trb_transfer_length;

    return true;
}

bool xHCI::configure_endpoints(const SharedPointer<xhci_device>& device, const vector<usb_endpoint_desc>& endpoints)
{
    xhci_input_control_context32* input_ctrl_ctx = device->get_input_control_ctx();
    memset(input_ctrl_ctx, 0, sizeof(*input_ctrl_ctx));

    xhci_slot_context32* slot_ctx = device->get_input_slot_ctx();
    uint8_t max_dci = slot_ctx->context_entries;

    for (const auto& desc : endpoints)
    {
        const uint8_t ep_num = desc.address & 0xF;
        const bool is_in = (desc.address & 0x80) != 0;
        const uint8_t dci = endpoint_dci(ep_num, is_in);

        if (dci > max_dci)
            max_dci = dci;

        if (!device->has_transfer_ring(dci))
            device->create_transfer_ring(dci, XHCI_TRANSFER_RING_TRB_COUNT);

        xhci_endpoint_context32* ep_ctx = device->get_input_ep_ctx(dci);
        memset(ep_ctx, 0, sizeof(*ep_ctx));

        const uint8_t transfer_type = desc.attributes & 0x3;
        ep_ctx->endpoint_type = transfer_type == 2 ? (is_in ? XHCI_ENDPOINT_TYPE_BULK_IN : XHCI_ENDPOINT_TYPE_BULK_OUT)
                               : transfer_type == 3 ? (is_in ? XHCI_ENDPOINT_TYPE_INTERRUPT_IN : XHCI_ENDPOINT_TYPE_INTERRUPT_OUT)
                                                     : (is_in ? XHCI_ENDPOINT_TYPE_ISOCHRONOUS_IN : XHCI_ENDPOINT_TYPE_ISOCHRONOUS_OUT);
        ep_ctx->max_packet_size = desc.max_packet_size;
        ep_ctx->error_count = 3;
        ep_ctx->average_trb_length = desc.max_packet_size;
        ep_ctx->interval = desc.xhci_interval;
        ep_ctx->transfer_ring_dequeue_ptr = (device->get_transfer_ring(dci)->get_physical_base() & ~0xFULL) | 1 /* DCS */;

        input_ctrl_ctx->add_flags |= 1u << dci;
    }

    slot_ctx->context_entries = max_dci;
    input_ctrl_ctx->add_flags |= 1u << 0; // A0: slot context (context_entries may have changed)

    xhci_trb_t trb{};
    trb.trb_type = XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_CMD;
    trb.parameter = device->get_input_ctx_physical_addr();
    trb.control |= static_cast<uint32_t>(device->get_slot()) << XHCI_TRB_CMD_SLOT_ID_SHIFT;

    return send_command_trb(&trb) != nullptr;
}

Status xHCI::bulk_transfer(const SharedPointer<xhci_device>& device, uint8_t endpoint_address,
                           void* data, uint32_t length, uint32_t* actual_length)
{
    if (actual_length)
        *actual_length = 0;

    if (length == 0)
        return MAKE_ERR("empty length");

    const uint8_t ep_num = endpoint_address & 0xF;
    const bool is_in = (endpoint_address & 0x80) != 0;
    const uint8_t dci = endpoint_dci(ep_num, is_in);

    const SharedPointer<xhci_transfer_ring>& ring = device->get_transfer_ring(dci);
    if (!ring)
        return MAKE_ERR("failed to get transfer ring");

    const auto pt = Scheduler::get_current_page_tables();
    const uintptr_t phys_base = PHYS_ADDR(pt, (uintptr_t)data);

    uint32_t offset = 0;
    while (offset < length)
    {
        const uint32_t chunk = length - offset > XHCI_MAX_NORMAL_TRB_TRANSFER_LENGTH
            ? XHCI_MAX_NORMAL_TRB_TRANSFER_LENGTH : length - offset;
        const bool last_chunk = offset + chunk == length;

        xhci_trb_t trb{};
        trb.trb_type = XHCI_TRB_TYPE_NORMAL;
        trb.parameter = phys_base + offset;
        trb.status = chunk;
        if (last_chunk)
            trb.interrupt_on_completion = 1;
        else
            trb.chain_bit = 1;
        ring->enqueue(&trb);

        offset += chunk;
    }

    doorbell_manager->ring_doorbell(device->get_slot(), dci);

    const xhci_transfer_event_trb_t* event = wait_for_transfer_event();
    if (!event)
        return MAKE_ERR("transfer event not received");

    if (event->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS &&
        event->completion_code != XHCI_TRB_COMPLETION_CODE_SHORT_PACKET)
        return MAKE_ERR("invalid completion code, got 0x%x", event->completion_code);

    if (actual_length)
        *actual_length = length - event->trb_transfer_length;

    return Status::success();
}

void xHCI::setup_device(uint8_t port_num)
{
    const xhci_portsc_register portsc = read_portsc_reg(port_num);
    const uint8_t speed = portsc.port_speed;

    const uint8_t slot_id = enable_device_slot();
    if (!slot_id)
    {
        printf_warn("Failed to enable a device slot for port %i", port_num);
        return;
    }

    if (!create_device_context(slot_id))
    {
        printf_warn("Failed to create a device context for slot %i", slot_id);
        return;
    }

    SharedPointer<xhci_device> device = { new xhci_device(port_num + 1, slot_id, speed, m_64byte_context_size) };

    if (!address_device(device))
    {
        printf_warn("Failed to address device on port %i (slot %i)", port_num, slot_id);
        return;
    }

    // Read the first 8 bytes of the Device Descriptor to learn the device's real
    // bMaxPacketSize0. Only Full-Speed devices can actually differ from our speed-based guess
    // (xHci Spec Section 4.3); for the other speeds this is purely a smoke test that the
    // control pipe works.
    void* desc_buf = Memory::physically_aligned_malloc(8, 8, PAGE_SIZE);
    if (desc_buf)
    {
        const usb_device_request get_desc_req = {
            .bm_request_type = 0x80, // Device-to-host | Standard | Device
            .b_request = USB_REQUEST_GET_DESCRIPTOR,
            .w_value = static_cast<uint16_t>(USB_DESCRIPTOR_TYPE_DEVICE << 8), // Descriptor Type = Device, Index = 0
            .w_index = 0,
            .w_length = 8
        };

        uint32_t actual_length = 0;
        if (control_transfer(device, get_desc_req, desc_buf, &actual_length) && actual_length == 8)
        {
            const uint8_t real_mps0 = static_cast<uint8_t*>(desc_buf)[7];
            xhci_endpoint_context32* ep0_ctx = device->get_input_control_ep_ctx();

            if (speed == XHCI_USB_SPEED_FULL_SPEED && real_mps0 && real_mps0 != ep0_ctx->max_packet_size)
            {
                ep0_ctx->max_packet_size = real_mps0;
                if (!evaluate_context(device))
                    printf_warn("Failed to update control endpoint max packet size for slot %i", slot_id);
            }
        }
        else
            printf_warn("Failed to read device descriptor header for slot %i", slot_id);
    }

    devices.push_back(device);

    // printf_info("USB device ready on port %i: slot=%i speed=%s max_packet_size0=%i",
    //             port_num, slot_id, _usb_speed_to_string(speed), device->get_input_control_ep_ctx()->max_packet_size);
}

xHCI* xHCI::get_instance()
{
    static xHCI* instance = nullptr;

    if (instance)
        return instance;
    if (PCI::xCHI.bus == (uint8_t)-1u)
        irrecoverable_error("No xCHI controller found, cannot continue");
    return instance = new xHCI(PCI::xCHI);
}

void xHCI::start()
{
    if (!start_host_controller())
        irrecoverable_error("xHCI controller failed to start");

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
        {
            // printf_info("Device connected on port %i - %s", port_num, _usb_speed_to_string(portsc.port_speed));
            setup_device(port_num);
        }
        else
            printf_warn("Failed to reset port %i after device connection", port_num);
    }
}

void xHCI::fire([[maybe_unused]] cpu_state_t* cpu_state, [[maybe_unused]] stack_state_t* stack_state)
{
    process_events();
    acknowledge_irq(0);
}
