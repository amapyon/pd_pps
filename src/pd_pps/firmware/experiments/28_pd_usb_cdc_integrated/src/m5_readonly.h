#ifndef M5_READONLY_H
#define M5_READONLY_H
#include <stdint.h>
#include <stddef.h>
#include "m5_pd_snapshot.h"

/* Protocol v7 DRAFT: one ASCII response, LF included <=63 bytes, one USB short packet.
 * No source/policy writes, I2C access, PD commands, or HAL calls in these modules. */
#define M5_WIRE_LIMIT 63u
#define M5_MAX_REQUEST 63u

typedef struct {
    uint32_t bus_resets;
    uint32_t tx_timeouts;
    uint32_t reconnects;
} m5_usb_health_t;

/* Body excludes LF and optional CR. This function ALWAYS returns 1..63 bytes
 * including a terminal LF; out has capacity 64 and is not C-string terminated. */
size_t m5_readonly_reply(const m5_pd_snapshot_t *snap,
                         const m5_usb_health_t *health,
                         const uint8_t *body, size_t length,
                         char out[64]);
#endif
