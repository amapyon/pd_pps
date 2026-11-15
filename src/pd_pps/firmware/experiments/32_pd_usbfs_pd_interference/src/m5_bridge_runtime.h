#ifndef M5_BRIDGE_RUNTIME_H
#define M5_BRIDGE_RUNTIME_H
#include "m5_session.h"
#include "m5_pd_snapshot.h"
/* To call only after the time-critical PD17 foreground service and any
 * bounded sensor task. Do NOT call from USBPD/USBFS interrupt. */
bool m5_bridge_publish_pd17(m5_session_t *session,const pe_t *pe,
                           const pd_sensor_reading_t *reading,
                           bool sensor_fresh,const m5_usb_health_t *health);
#endif
