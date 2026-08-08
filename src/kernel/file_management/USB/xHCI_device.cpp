#include "xHCI_device.h"

#include "xHCI_mem.h"
#include "../../core/memory/memory.h"
#include <kstring.h>

[[noreturn]]
extern int irrecoverable_error(const char* format, ...);

xhci_device::xhci_device(uint8_t port, uint8_t slot, uint8_t speed, bool use_64byte_ctx) :
    m_port(port), m_slot(slot), m_speed(speed), m_use_64byte_ctx(use_64byte_ctx)
{
    alloc_input_ctx();
}

void xhci_device::alloc_input_ctx()
{
    const size_t input_ctx_size = m_use_64byte_ctx ? sizeof(xhci_input_context64) : sizeof(xhci_input_context32);

    m_input_ctx = Memory::physically_aligned_malloc(
        input_ctx_size,
        XHCI_INPUT_CONTROL_CONTEXT_ALIGNMENT,
        XHCI_INPUT_CONTROL_CONTEXT_BOUNDARY
    );
    if (!m_input_ctx)
        irrecoverable_error("%s: physically_aligned_malloc failed", __func__);

    memset(m_input_ctx, 0, input_ctx_size);

    m_input_ctx_phys = PHYS_ADDR(Memory::page_tables, (uintptr_t)m_input_ctx);
}

xhci_input_control_context32* xhci_device::get_input_control_ctx()
{
    if (m_use_64byte_ctx)
        return reinterpret_cast<xhci_input_control_context32*>(&static_cast<xhci_input_context64*>(m_input_ctx)->control_context);
    return &static_cast<xhci_input_context32*>(m_input_ctx)->control_context;
}

xhci_slot_context32* xhci_device::get_input_slot_ctx()
{
    if (m_use_64byte_ctx)
        return reinterpret_cast<xhci_slot_context32*>(&static_cast<xhci_input_context64*>(m_input_ctx)->device_context.slot_context);
    return &static_cast<xhci_input_context32*>(m_input_ctx)->device_context.slot_context;
}

xhci_endpoint_context32* xhci_device::get_input_control_ep_ctx()
{
    if (m_use_64byte_ctx)
        return reinterpret_cast<xhci_endpoint_context32*>(&static_cast<xhci_input_context64*>(m_input_ctx)->device_context.control_ep_context);
    return &static_cast<xhci_input_context32*>(m_input_ctx)->device_context.control_ep_context;
}

xhci_endpoint_context32* xhci_device::get_input_ep_ctx(uint8_t dci)
{
    if (dci < 2 || dci > 31)
        irrecoverable_error("%s: invalid DCI %i", __func__, dci);

    const uint8_t ep_index = dci - 2;

    if (m_use_64byte_ctx)
        return reinterpret_cast<xhci_endpoint_context32*>(&static_cast<xhci_input_context64*>(m_input_ctx)->device_context.ep[ep_index]);
    return &static_cast<xhci_input_context32*>(m_input_ctx)->device_context.ep[ep_index];
}

const SharedPointer<xhci_transfer_ring>& xhci_device::get_transfer_ring(uint8_t dci) const
{
    if (dci < 1 || dci >= MAX_DCI)
        irrecoverable_error("%s: invalid DCI %i", __func__, dci);

    return m_transfer_rings[dci];
}

bool xhci_device::has_transfer_ring(uint8_t dci) const
{
    return dci >= 1 && dci < MAX_DCI && m_transfer_rings[dci].get() != nullptr;
}

const SharedPointer<xhci_transfer_ring>& xhci_device::create_transfer_ring(uint8_t dci, size_t max_trbs)
{
    if (dci < 1 || dci >= MAX_DCI)
        irrecoverable_error("%s: invalid DCI %i", __func__, dci);

    m_transfer_rings[dci] = { new xhci_transfer_ring(max_trbs) };
    return m_transfer_rings[dci];
}
