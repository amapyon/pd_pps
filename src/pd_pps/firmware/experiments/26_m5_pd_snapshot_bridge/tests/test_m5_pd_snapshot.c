#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/m5_pd_snapshot.h"

static pe_t pps_ready(void)
{
    pe_t p = {0};
    p.state = PE_READY;
    p.contract_valid = true;
    p.pps_active = true;
    p.caps_fresh = true;
    p.caps_epoch = 1;
    p.caps.count = 6;
    p.caps.pdo[0] = 0x0801912cu;
    p.caps.pdo[1] = 0x0002d12cu;
    p.caps.pdo[2] = 0x0003c12cu;
    p.caps.pdo[3] = 0x0004b12cu;
    p.caps.pdo[4] = 0x0006412cu;
    p.caps.pdo[5] = 0xc1a4213cu;
    p.pdo_index = 6;
    p.requested_mv = 7000;
    p.requested_ma = 1000;
    return p;
}

int main(void)
{
    m5_pd_snapshot_t s;
    pe_t p = pps_ready();
    pe_t before = p;
    assert(m5_pd_snapshot_capture(&p, NULL, false, &s));
    assert(s.state == PE_READY && s.contract_valid && s.pps_active);
    assert(s.caps_count == 6 && s.caps_epoch == 1);
    assert(s.pdo_index == 6 && s.requested_mv == 7000 && s.requested_ma == 1000);
    assert(s.raw_pdos[5] == 0xc1a4213cu && s.raw_pdos[0] == 0x0801912cu);
    assert(!s.ina_valid && !s.adc_valid);
    assert(memcmp(&p, &before, sizeof(p)) == 0);  /* no mutation */
    puts("01/12 PASS READY + real PD17 fixture, no mutation");

    p.state = PE_DETACHED;
    assert(m5_pd_snapshot_capture(&p, NULL, false, &s));
    assert(s.state == PE_DETACHED && !s.contract_valid && !s.caps_valid && s.caps_count == 0 && s.pdo_index == 0);
    assert(s.raw_pdos[5] == 0 && s.requested_mv == 0);
    puts("02/12 PASS detach discards stale caps/contract");

    p = pps_ready(); p.state = PE_RECOVERY; p.faults = 9;
    assert(m5_pd_snapshot_capture(&p, NULL, false, &s));
    assert(!s.contract_valid && !s.caps_valid && s.faults == 9);
    puts("03/12 PASS recovery hides stale values and retains fault");

    p = pps_ready(); p.caps_fresh = false; p.caps_epoch = 2;
    assert(m5_pd_snapshot_capture(&p, NULL, false, &s));
    assert(!s.contract_valid && !s.caps_valid && s.caps_epoch == 2);
    puts("04/12 PASS stale Source Caps hidden");

    p = pps_ready(); p.pdo_index = 7;
    assert(m5_pd_snapshot_capture(&p, NULL, false, &s));
    assert(!s.contract_valid && s.caps_valid && s.pdo_index == 0);
    puts("05/12 PASS out of range index suppressed");

    p = pps_ready(); p.caps.count = 8;
    memset(&s, 0xAB, sizeof(s));
    assert(!m5_pd_snapshot_capture(&p, NULL, false, &s));
    assert(s.state == PE_DETACHED && !s.caps_valid && !s.contract_valid && !s.ina_valid);
    puts("06/12 PASS corrupt cap count fails closed");

    p = pps_ready(); p.state = PE_WAIT_ACCEPT;
    assert(m5_pd_snapshot_capture(&p, NULL, false, &s));
    assert(!s.contract_valid && s.caps_valid && !s.pps_active && s.requested_mv == 0);
    puts("07/12 PASS unconfirmed request not called contract");

    p = pps_ready(); p.pps_active = false; p.pdo_index = 1; p.caps.count=1;
    p.requested_mv=5000; p.requested_ma=1000;
    assert(m5_pd_snapshot_capture(&p, NULL, false, &s));
    assert(s.contract_valid && !s.pps_active && s.pdo_index == 1 && s.requested_mv == 5000);
    puts("08/12 PASS fixed source contract");

    pd_sensor_reading_t sensor = {0};
    sensor.ina_ok = true;
    sensor.ina_bus_mv = 6965;
    sensor.ina_current_ua = -50123;
    sensor.ina_power_mw = -349;
    sensor.adc_ok = true;
    sensor.adc_calibrated_range = true;
    sensor.adc_cal_mv = 7001;
    p = pps_ready();
    assert(m5_pd_snapshot_capture(&p, &sensor, true, &s));
    assert(s.ina_valid && s.ina_bus_mv == 6965 && s.ina_current_ua == -50123 && s.ina_power_mw == -349);
    assert(s.adc_valid && s.adc_bus_mv == 7001);
    assert(s.requested_ma == 1000); /* not measured current */
    puts("09/12 PASS current units, sign, request vs actual distinct");

    assert(m5_pd_snapshot_capture(&p, &sensor, false, &s));
    assert(!s.ina_valid && !s.adc_valid && s.ina_bus_mv == 0);
    puts("10/12 PASS stale sensor invalidated");

    sensor.ina_ok = false; sensor.adc_calibrated_range=false;
    assert(m5_pd_snapshot_capture(&p, &sensor, true, &s));
    assert(!s.ina_valid && !s.adc_valid);
    puts("11/12 PASS sensor I/O fail and invalid ADC calibration");

    memset(&s, 0xCD, sizeof(s));
    assert(!m5_pd_snapshot_capture(NULL, NULL, false, &s));
    assert(!s.contract_valid && s.state == PE_DETACHED);
    assert(!m5_pd_snapshot_capture(&p, NULL, false, NULL));
    puts("12/12 PASS NULL input fails closed");
    puts("PASS: M5-2 C snapshot bridge (12/12)");
    return 0;
}
