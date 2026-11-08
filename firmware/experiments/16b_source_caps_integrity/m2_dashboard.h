#ifndef M2_DASHBOARD_H
#define M2_DASHBOARD_H

#include "m2_ssd1306.h"

typedef struct {
    int32_t requested_mv;       /* PD requested voltage; NOT a measurement */
    uint16_t requested_ma;      /* PD requested current; NOT measured load I */
    bool pd_ready;
    bool ina_valid;
    uint32_t bus_mv;          /* INA226 BUS 1.25mV/LSB */
    int32_t load_ua;          /* shunt_raw * 250uA with nominal R010 */
    int32_t load_power_mw;    /* bus_mv * load_ua / 1,000,000 */
} m2_dashboard_values_t;

typedef struct {
    m2_ssd1306_t oled;
    m2_dashboard_values_t values;
    uint32_t last_render_ms;
    bool rendered_once;
    bool display_fault;
} m2_dashboard_t;

/* Call after power-up and >=100ms after 3.3V/OLED power becomes stable. */
bool m2_dashboard_init(m2_dashboard_t *ui, m2_i2c_write_fn write,
                       void *context); /* auto-detect 0x3C or 0x3D */
/* Supply existing PD state, DO NOT modify PD behavior. */
void m2_dashboard_set_pd(m2_dashboard_t *ui, int32_t requested_mv,
                         uint16_t requested_ma, bool pd_ready);
/* Feed EXISTING INA226 raw readings, e.g. where serial 'm' is populated. */
void m2_dashboard_set_ina_raw(m2_dashboard_t *ui, uint16_t bus_raw,
                              int16_t shunt_raw, bool valid);
/* Call at least every ~1-10ms at the tail of the existing main loop,
 * after PD/serial processing. Each invocation does <=1 I2C transaction.
 * Screen content is scheduled at most once per 1000 ms. */
void m2_dashboard_service(m2_dashboard_t *ui, uint32_t now_ms);

#endif
