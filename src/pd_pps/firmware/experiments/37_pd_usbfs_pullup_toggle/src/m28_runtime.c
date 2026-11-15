#include "m28_runtime.h"

void m28_runtime_init(m28_runtime_t *r) {
    if (!r) return;
    m5_session_init(&r->session);
    m5_usb_recovery_nb_init(&r->recovery);
    m28_transport_init(&r->transport);
    r->last_reset_seq = 0u;
    r->last_complete_seq = 0u;
    r->discarded_overflows = 0u;
}

m28_action_t m28_runtime_poll(m28_runtime_t *r, uint32_t now_ms,
    const pe_t *pe, const pd_sensor_reading_t *reading, bool reading_fresh,
    const m5_usb_io_t *io, bool usb_configured) {
    if (!r) return M28_NO_ACTION;
    const uint32_t reset_seen = r->transport.bus_reset_seq;
    if (reset_seen != r->last_reset_seq) {
        r->last_reset_seq = reset_seen;
        r->last_complete_seq = r->transport.in_complete_seq;
        m5_usb_recovery_nb_bus_reset(&r->recovery);
        (void)m5_usb_recovery_nb_poll(&r->recovery, now_ms);
        m5_session_usb_reset(&r->session);
        m28_transport_discard(&r->transport);
        return M28_NO_ACTION;
    }
    if (r->transport.overflow) {
        /* Do not interpret a truncated command as a valid one. */
        r->discarded_overflows++;
        m5_session_usb_reset(&r->session);
        m28_transport_discard(&r->transport);
        return M28_NO_ACTION;
    }
    if (r->last_complete_seq != r->transport.in_complete_seq) {
        r->last_complete_seq = r->transport.in_complete_seq;
        m5_usb_recovery_nb_complete(&r->recovery);
    }
    const m5_usb_recovery_action_t action = m5_usb_recovery_nb_poll(&r->recovery, now_ms);
    if (action == M5_USB_EVENT_DETACH) {
        m5_session_usb_reset(&r->session);
        m28_transport_discard(&r->transport);
        return M28_STOP_USB;
    }
    if (action == M5_USB_EVENT_REINIT) {
        m5_session_usb_reset(&r->session);
        m28_transport_discard(&r->transport);
        return M28_START_USB;
    }
    if (action == M5_USB_EVENT_DROP_PENDING) {
        m5_session_usb_reset(&r->session);
        m28_transport_discard(&r->transport);
        return M28_NO_ACTION;
    }
    if (r->recovery.state != M5_USB_ACTIVE) return M28_NO_ACTION;
    const m5_usb_health_t health = {
        r->recovery.bus_resets, r->recovery.tx_timeouts, r->recovery.reconnects
    };
    (void)m5_bridge_publish_pd17(&r->session, pe, reading, reading_fresh, &health);
    if (usb_configured) m5_usb_io_poll(&r->session, io, false);
    return M28_NO_ACTION;
}
