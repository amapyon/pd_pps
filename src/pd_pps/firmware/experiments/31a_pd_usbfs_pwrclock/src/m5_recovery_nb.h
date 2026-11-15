#ifndef M5_RECOVERY_NB_H
#define M5_RECOVERY_NB_H
#include <stdint.h>
#include <stdbool.h>
/* Portable, nonblocking replacement DESIGN for M8F's 150ms Delay_Ms in the
 * timeout-triggered USB re-enumeration path. NO hardware calls here. */
#define M5_EP3_TIMEOUT_MS 2500u
#define M5_USB_DETACH_HOLD_MS 150u

typedef enum {M5_USB_ACTIVE=0,M5_USB_DISCONNECTED_WAIT=1} m5_usb_recovery_state_t;
typedef enum {
    M5_USB_EVENT_NONE=0,
    M5_USB_EVENT_DROP_PENDING=1,  /* discard queued command and reply */
    M5_USB_EVENT_DETACH=2,        /* disable USB port + pullup, never block */
    M5_USB_EVENT_REINIT=3         /* USBFSSetup now, from foreground */
} m5_usb_recovery_action_t;
typedef struct {
    m5_usb_recovery_state_t state;
    uint32_t submit_ms;
    uint32_t disconnected_at_ms;
    uint32_t bus_resets;
    uint32_t seen_bus_resets;
    uint32_t tx_timeouts;
    uint32_t reconnects;
    bool in_flight;
} m5_usb_recovery_nb_t;
void m5_usb_recovery_nb_init(m5_usb_recovery_nb_t *s);
/* ISR event writers must be synchronized with foreground on the target.
 * Call on IN begin before arming ACK, then fail/succeed on submit result;
 * completion clears in_flight. Actual IRQ synchronization is not in this kit. */
void m5_usb_recovery_nb_submit_begin(m5_usb_recovery_nb_t *s,uint32_t now_ms);
void m5_usb_recovery_nb_submit_result(m5_usb_recovery_nb_t *s,bool accepted);
void m5_usb_recovery_nb_complete(m5_usb_recovery_nb_t *s);
void m5_usb_recovery_nb_bus_reset(m5_usb_recovery_nb_t *s);
m5_usb_recovery_action_t m5_usb_recovery_nb_poll(m5_usb_recovery_nb_t *s,uint32_t now_ms);
#endif
