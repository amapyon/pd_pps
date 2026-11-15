#ifndef M28_USB_H
#define M28_USB_H
#include <stdbool.h>
#include <stdint.h>
#include "pd_policy.h"
#include "pd_sensors.h"

/* Standalone PD17+USBFS hardware adapter. Foreground services at most one
 * EP3 submission OR one command byte per loop; never calls PD APIs. */
void m28_usb_init(void);
void m28_usb_service(uint32_t now_ms, const pe_t *pe,
                     const pd_sensor_reading_t *reading, bool reading_fresh);
void m28_usb_on_bus_reset_irq(void);
#endif
