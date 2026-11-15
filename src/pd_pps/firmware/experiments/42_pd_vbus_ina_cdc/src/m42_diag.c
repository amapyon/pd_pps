#include "m42_diag.h"
#include "pd_vbus_monitor.h"

static pd_vbus_point_t frozen_points[M42_STOP_SAMPLES];
static uint8_t frozen_count;
static bool frozen;

void m42_diag_new_attach(void) {
    frozen_count = 0u;
    frozen = false;
}

void m42_diag_capture_first_stop(const pd_stop_diag_t *stop) {
    if (frozen || !stop || !stop->active || !stop->found) return;
    const pd_vbus_history_t *h = pd_vbus_monitor_history();
    uint16_t total = h->count;
    uint16_t take = total > M42_STOP_SAMPLES ? M42_STOP_SAMPLES : total;
    uint16_t start = total - take;
    for (uint16_t i = 0; i < take; ++i) {
        pd_vbus_point_t point;
        if (!pd_vbus_history_get(h, (uint16_t)(start + i), &point)) break;
        frozen_points[frozen_count++] = point;
    }
    frozen = true;
}

bool m42_diag_frozen(void) { return frozen; }
uint8_t m42_diag_stop_count(void) { return frozen_count; }
bool m42_diag_stop_get(uint8_t index, pd_vbus_point_t *out) {
    if (!out || !frozen || index >= frozen_count) return false;
    *out = frozen_points[index];
    return true;
}
