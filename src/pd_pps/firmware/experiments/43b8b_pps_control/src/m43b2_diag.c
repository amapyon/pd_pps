/* M43B-2 low-overhead observational counters. */
#include "m43b2_diag.h"
/* IRQ code owns updates during attach; the foreground reads only after
 * detach. 32-bit increments/samples are single instructions on RV32. */
static volatile m43b2_diag_t d;
static volatile uint32_t usbfs_start_tick;
static volatile uint8_t active, usbfs_active;
void m43b2_diag_attach(void) {
    active = 0u;
    m43b2_diag_t zero = {0};
    d = zero;
    usbfs_start_tick = 0u;
    usbfs_active = 0u;
    active = 1u;
}
void m43b2_diag_detach(void) { active = 0u; usbfs_active = 0u; }
void m43b2_diag_usbfs_enter(bool crc_window, uint32_t tick) {
    if (!active) return;
    usbfs_start_tick = tick;
    usbfs_active = 1u;
    d.usbfs_irq++;
    if (crc_window) d.usbfs_crc_window++;
}
uint32_t m43b2_diag_usbfs_crc_count(void) { return d.usbfs_crc_window; }
void m43b2_diag_usbfs_exit(uint32_t tick) {
    if (!active || !usbfs_active) return;
    uint32_t cycles = tick - usbfs_start_tick;
    if (cycles > d.usbfs_max_cycles) d.usbfs_max_cycles = cycles;
    usbfs_active = 0u;
}
void m43b2_diag_pd_irq(uint16_t status) {
    if (!active) return;
    d.pd_irq++;
    if (usbfs_active) d.pd_preempted_usbfs++;
    d.pd_status_or |= (uint32_t)status;
    /* A status-bit sample, NOT a newly asserted interrupt or CRC error. */
    if (status & (1u << 2)) d.buf_err_status_samples++;
    if (status & (1u << 7)) d.tx_end++;
    if (status & (1u << 5)) d.rx_act++;
}
void m43b2_diag_rx_arm(uint32_t cycles) {
    if (!active) return;
    d.rx_arm_count++;
    d.rx_arm_last_cycles = cycles;
    if (cycles > d.rx_arm_max_cycles) d.rx_arm_max_cycles = cycles;
}
void m43b2_diag_goodcrc(bool match, bool armed, uint32_t cycles) {
    if (!active) return;
    d.goodcrc_seen++;
    if (match) d.goodcrc_match++; else d.goodcrc_mismatch++;
    if (armed) {
        d.goodcrc_window_samples++;
        if (cycles > d.goodcrc_max_cycles) d.goodcrc_max_cycles = cycles;
    }
}
void m43b2_diag_retry(void) { if (active) d.retry++; }
void m43b2_diag_tx_failed(void) { if (active) d.tx_failed++; }
void m43b2_diag_snapshot(m43b2_diag_t *out) {
    if (out == 0) return;
    /* Called only detached, after all ISR producers have been gated.
     * Never used for an attached or cross-interrupt coherent snapshot. */
    out->pd_irq = d.pd_irq;
    out->usbfs_irq = d.usbfs_irq;
    out->usbfs_crc_window = d.usbfs_crc_window;
    out->pd_preempted_usbfs = d.pd_preempted_usbfs;
    out->usbfs_max_cycles = d.usbfs_max_cycles;
    out->pd_status_or = d.pd_status_or;
    out->buf_err_status_samples = d.buf_err_status_samples;
    out->tx_end = d.tx_end;
    out->rx_act = d.rx_act;
    out->rx_arm_count = d.rx_arm_count;
    out->rx_arm_max_cycles = d.rx_arm_max_cycles;
    out->rx_arm_last_cycles = d.rx_arm_last_cycles;
    out->goodcrc_seen = d.goodcrc_seen;
    out->goodcrc_match = d.goodcrc_match;
    out->goodcrc_mismatch = d.goodcrc_mismatch;
    out->goodcrc_window_samples = d.goodcrc_window_samples;
    out->goodcrc_max_cycles = d.goodcrc_max_cycles;
    out->retry = d.retry;
    out->tx_failed = d.tx_failed;
}
