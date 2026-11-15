#ifndef M28_RUNTIME_H
#define M28_RUNTIME_H
#include "m5_bridge_runtime.h"
#include "m5_recovery_nb.h"
#include "m5_usb_io.h"
#include "m28_transport.h"

typedef enum {
    M28_NO_ACTION = 0,
    M28_STOP_USB = 1,
    M28_START_USB = 2
} m28_action_t;
typedef struct {
    m5_session_t session;
    m5_usb_recovery_nb_t recovery;
    m28_transport_t transport;
    uint32_t last_reset_seq;
    uint32_t last_complete_seq;
    uint32_t discarded_overflows;
} m28_runtime_t;

void m28_runtime_init(m28_runtime_t *r);
/* One foreground iteration: never sleeps, allocates, prints or calls PD APIs. */
m28_action_t m28_runtime_poll(m28_runtime_t *r, uint32_t now_ms,
    const pe_t *pe, const pd_sensor_reading_t *reading, bool reading_fresh,
    const m5_usb_io_t *io, bool usb_configured);
#endif
