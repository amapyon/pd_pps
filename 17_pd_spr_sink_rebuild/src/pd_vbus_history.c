#include "pd_vbus_history.h"

void pd_vbus_history_clear(pd_vbus_history_t *h)
{
    h->head = 0u;
    h->count = 0u;
    h->total = 0u;
    h->overwritten = 0u;
    h->min_ready_raw = 0xffffu;
    h->max_ready_raw = 0u;
    h->ready_samples = 0u;
}

void pd_vbus_history_append(pd_vbus_history_t *h, uint32_t time_ms,
                            uint16_t raw, uint8_t flags)
{
    pd_vbus_point_t *p = &h->points[h->head];
    p->time_ms = time_ms;
    p->adc_raw = raw;
    p->flags = flags;
    p->reserved = 0u;
    h->head = (uint16_t)((h->head + 1u) % PD_VBUS_HISTORY_LEN);
    if (h->count == PD_VBUS_HISTORY_LEN) h->overwritten++;
    else h->count++;
    h->total++;
    if (flags & PD_VBUS_FLAG_READY) {
        h->ready_samples++;
        if (raw < h->min_ready_raw) h->min_ready_raw = raw;
        if (raw > h->max_ready_raw) h->max_ready_raw = raw;
    }
}

bool pd_vbus_history_get(const pd_vbus_history_t *h, uint16_t age,
                         pd_vbus_point_t *out)
{
    if (age >= h->count) return false;
    uint16_t oldest = (uint16_t)((h->head + PD_VBUS_HISTORY_LEN - h->count)
                                  % PD_VBUS_HISTORY_LEN);
    *out = h->points[(oldest + age) % PD_VBUS_HISTORY_LEN];
    return true;
}
