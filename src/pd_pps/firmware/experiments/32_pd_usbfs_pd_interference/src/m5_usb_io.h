#ifndef M5_USB_IO_H
#define M5_USB_IO_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "m5_session.h"

/* Foreground USB adapter. Connect to experiment-24 helpers in a FUTURE target
 * build. The actual USBFS/USBPD controller code is intentionally unchanged. */
typedef struct {
    /* Return true and pop one received byte. Must not pop if called without
     * RX data. Implement over the M8F interrupt-fed RX ring. */
    bool (*rx_pop)(void *context,uint8_t *out);
    /* Return 0 if endpoint accepted data; nonzero if busy/error.
     * The callback MUST copy data to the USB endpoint DMA buffer before
     * returning success; the source pointer is not owned by the driver.
     * A return of 0 means USBFS_SendEndpoint accepted submission, not that
     * the USB host ACKed the IN transaction. M8F recovery handles timeout. */
    int (*tx_submit)(void *context,const uint8_t *buf,size_t len);
    void *context;
} m5_usb_io_t;
/* Call after M8F_RecoveryPoll() and after PD17 service in the foreground.
 * On reset_event, caller MUST discard pre-reset ISR RX ring entries before
 * the next poll, AND call this with reset_event=true to clear protocol data.
 * A tick attempts at most one transmit or consumes at most one RX byte. */
void m5_usb_io_poll(m5_session_t *session,const m5_usb_io_t *io,bool reset_event);
#endif
