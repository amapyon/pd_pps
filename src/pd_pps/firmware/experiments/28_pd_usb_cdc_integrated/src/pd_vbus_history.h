#ifndef PD_VBUS_HISTORY_H
#define PD_VBUS_HISTORY_H
#include <stdbool.h>
#include <stdint.h>

/* Fixed-memory, overwriting ring. Oldest samples are dropped so the last
 * ~1.28 s of observations survive a cable pull at 10 ms cadence. */
#define PD_VBUS_HISTORY_LEN 128u
#define PD_VBUS_FLAG_ATTACHED 0x01u
#define PD_VBUS_FLAG_READY 0x02u
#define PD_VBUS_FLAG_HARD_RESET 0x04u

typedef struct {
    uint32_t time_ms;
    uint16_t adc_raw;
    uint8_t flags;
    uint8_t reserved;
} pd_vbus_point_t;

typedef struct {
    pd_vbus_point_t points[PD_VBUS_HISTORY_LEN];
    uint16_t head;
    uint16_t count;
    uint32_t total;
    uint32_t overwritten;
    uint16_t min_ready_raw;
    uint16_t max_ready_raw;
    uint32_t ready_samples;
} pd_vbus_history_t;

void pd_vbus_history_clear(pd_vbus_history_t *h);
void pd_vbus_history_append(pd_vbus_history_t *h, uint32_t time_ms,
                            uint16_t raw, uint8_t flags);
bool pd_vbus_history_get(const pd_vbus_history_t *h, uint16_t age,
                         pd_vbus_point_t *out);
#endif
