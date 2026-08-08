#pragma once

#include "xHCI_device_ctx.h"
#include "xHCI_rings.h"
#include "../../utils/shared_pointer.h"

// A USB device attached to an xHCI root hub port. Owns the Input Context handed to Address
// Device / Configure Endpoint / Evaluate Context Commands, and the per-endpoint Transfer Rings
// the controller reads TRBs from (indexed by Device Context Index - DCI 1 = default control
// endpoint, DCI 2-31 = the endpoints added later via xHCI::configure_endpoints()).
class xhci_device
{
public:
    // port is the 1-based root hub port number
    xhci_device(uint8_t port, uint8_t slot, uint8_t speed, bool use_64byte_ctx);

    [[nodiscard]] uint8_t get_port() const { return m_port; }
    [[nodiscard]] uint8_t get_slot() const { return m_slot; }
    [[nodiscard]] uint8_t get_speed() const { return m_speed; }

    [[nodiscard]] uintptr_t get_input_ctx_physical_addr() const { return m_input_ctx_phys; }

    xhci_input_control_context32* get_input_control_ctx();
    xhci_slot_context32* get_input_slot_ctx();
    xhci_endpoint_context32* get_input_control_ep_ctx();
    xhci_endpoint_context32* get_input_ep_ctx(uint8_t dci); // dci in [2, 31]

    // Returns the Transfer Ring for the given DCI (1 = control endpoint), or a null
    // SharedPointer if none has been created yet for this endpoint.
    [[nodiscard]] const SharedPointer<xhci_transfer_ring>& get_transfer_ring(uint8_t dci) const;
    [[nodiscard]] bool has_transfer_ring(uint8_t dci) const;

    // Allocates a fresh Transfer Ring for the given DCI (1-31), replacing any existing one.
    const SharedPointer<xhci_transfer_ring>& create_transfer_ring(uint8_t dci, size_t max_trbs);

private:
    static constexpr uint8_t MAX_DCI = 32; // valid indices are [1, 31], index 0 is unused

    const uint8_t m_port;
    const uint8_t m_slot;
    const uint8_t m_speed;
    const bool    m_use_64byte_ctx;

    void*         m_input_ctx = nullptr;
    uintptr_t     m_input_ctx_phys = 0;

    SharedPointer<xhci_transfer_ring> m_transfer_rings[MAX_DCI];

    void alloc_input_ctx();
};
