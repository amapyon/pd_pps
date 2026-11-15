#ifndef M5_SESSION_H
#define M5_SESSION_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "m5_readonly.h"

/* Main-loop only API. IRQ is permitted to set a reset flag and write to the
 * separate USB RX ring. This module never calls USBFS/PD/INA226 directly. */
typedef struct {
    m5_pd_snapshot_t snapshot;
    m5_usb_health_t health;
    bool snapshot_ready;
    uint8_t command[64];
    uint8_t command_len;
    bool line_bad;
    char reply[64];
    uint8_t reply_len,reply_sent;
    uint32_t commands_completed;
    uint32_t rejected_lines;
} m5_session_t;

void m5_session_init(m5_session_t *s);
void m5_session_publish(m5_session_t *s,const m5_pd_snapshot_t *snapshot,
                        const m5_usb_health_t *health);
/* On USB RESET/reconnect: forget a partial command, queued reply and cached
 * snapshot. The caller must skip stale bytes in its RX ISR ring separately. */
void m5_session_usb_reset(m5_session_t *s);
/* Feed at most one RX byte per main-loop iteration. If a reply is pending,
 * return false and do NOT consume the caller's byte from its ring. */
bool m5_session_rx(m5_session_t *s,uint8_t ch);
/* Peek the next unsent response bytes. After USBFS_SendEndpoint success,
 * explicitly advance with m5_session_tx_accepted(). A failed send leaves
 * bytes pending; never advance until submission was accepted. */
size_t m5_session_tx_peek(const m5_session_t *s,const char **data);
void m5_session_tx_accepted(m5_session_t *s,size_t size);
bool m5_session_has_reply(const m5_session_t *s);
#endif
