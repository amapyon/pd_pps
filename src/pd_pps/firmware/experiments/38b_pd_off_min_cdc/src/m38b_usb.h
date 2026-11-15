#ifndef M38B_USB_H
#define M38B_USB_H
#include <stdint.h>
/* USBPD-disabled control. No PD structures, interrupts, or services. */
void m38b_usb_init(void);
void m38b_usb_service(void);
void m38b_usb_on_irq(uint16_t intfgst);
void m38b_usb_on_bus_reset_irq(void);
#endif
