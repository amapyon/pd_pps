#include "m43b2_diag.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    m43b2_diag_t d;
    m43b2_diag_usbfs_enter(true, 2u); /* inactive: ignored */
    m43b2_diag_attach();
    m43b2_diag_usbfs_enter(true, 0xfffffff0u);
    m43b2_diag_pd_irq(0xa4u); /* RX_ACT and TX_END; BUF_ERR sample */
    m43b2_diag_usbfs_exit(0x20u); /* wrap-safe: 0x30 cycles */
    m43b2_diag_rx_arm(17u);
    m43b2_diag_rx_arm(5u);
    m43b2_diag_goodcrc(true,true,140u);
    m43b2_diag_goodcrc(false,false,0u);
    m43b2_diag_retry();
    m43b2_diag_tx_failed();
    m43b2_diag_detach();
    m43b2_diag_usbfs_enter(true,0u); /* detached: ignored */
    m43b2_diag_snapshot(&d);
    assert(d.usbfs_irq == 1u && d.usbfs_crc_window == 1u);
    assert(d.pd_irq == 1u && d.pd_preempted_usbfs == 1u);
    assert(d.usbfs_max_cycles == 48u && d.pd_status_or == 0xa4u);
    assert(d.buf_err_status_samples == 1u);
    assert(d.tx_end == 1u && d.rx_act == 1u);
    assert(d.rx_arm_count == 2u && d.rx_arm_max_cycles == 17u && d.rx_arm_last_cycles == 5u);
    assert(d.goodcrc_seen == 2u && d.goodcrc_match == 1u && d.goodcrc_mismatch == 1u);
    assert(d.goodcrc_window_samples == 1u && d.goodcrc_max_cycles == 140u);
    assert(d.retry == 1u && d.tx_failed == 1u);
    m43b2_diag_attach();
    m43b2_diag_detach();
    m43b2_diag_snapshot(&d);
    assert(d.usbfs_irq == 0u && d.pd_irq == 0u && d.retry == 0u);
    puts("M43B2 DIAG HOST PASS");
    return 0;
}
