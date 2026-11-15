#ifndef M43B2_DIAG_H
#define M43B2_DIAG_H
#include <stdbool.h>
#include <stdint.h>
/* M43B-2: counters are reset on PD CC attach and frozen on CC detach.
 * All entry functions are constant-time; no printf, GPIO, DMA or PD
 * peripheral access in the diagnostic code. Timing uses low 32-bit
 * SysTick cycle values supplied by ISR callers (wrap-safe subtraction). */
typedef struct {
    uint32_t pd_irq;
    uint32_t usbfs_irq;
    uint32_t usbfs_crc_window;
    uint32_t pd_preempted_usbfs;
    uint32_t usbfs_max_cycles;
    uint32_t pd_status_or;
    uint32_t buf_err_status_samples;
    uint32_t tx_end;
    uint32_t rx_act;
    uint32_t rx_arm_count;
    uint32_t rx_arm_max_cycles;
    uint32_t rx_arm_last_cycles;
    uint32_t goodcrc_seen;
    uint32_t goodcrc_match;
    uint32_t goodcrc_mismatch;
    uint32_t goodcrc_window_samples;
    uint32_t goodcrc_max_cycles;
    uint32_t retry;
    uint32_t tx_failed;
} m43b2_diag_t;
void m43b2_diag_attach(void);
void m43b2_diag_detach(void);
void m43b2_diag_usbfs_enter(bool crc_window, uint32_t tick);
void m43b2_diag_usbfs_exit(uint32_t tick);
uint32_t m43b2_diag_usbfs_crc_count(void); /* M43B5: atomic read-only counter */
void m43b2_diag_pd_irq(uint16_t status);
void m43b2_diag_rx_arm(uint32_t cycles);
void m43b2_diag_goodcrc(bool match, bool armed, uint32_t cycles);
void m43b2_diag_retry(void);
void m43b2_diag_tx_failed(void);
void m43b2_diag_snapshot(m43b2_diag_t *out);
#endif
