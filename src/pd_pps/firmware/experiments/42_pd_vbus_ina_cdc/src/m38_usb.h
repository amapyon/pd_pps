#ifndef M38_USB_H
#define M38_USB_H
#include <stdbool.h>
#include <stdint.h>
#include "pd_policy.h"
#include "pd_stability.h"
#include "pd_stop_diag.h"

/* Experimental minimalist USB CDC, no send timeout / USBFS reinitialisation.
 * Do not call from a USB IRQ. PD safety processing always runs first. */
void m38_usb_init(void);
void m38_usb_service(uint32_t now_ms, const pe_t *pe, bool attached,
                     const pd_stability_t *stability,
                     const pd_stop_diag_t *stop);
/* IRQ hooks; never call PD or blocking functions. */
void m38_usb_on_irq(uint16_t intfgst);
void m38_usb_on_bus_reset_irq(void);
#endif
