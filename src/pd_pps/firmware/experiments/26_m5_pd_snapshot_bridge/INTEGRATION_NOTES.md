# M5-2 interface mapping and review notes

| PD17 source | C snapshot | M5-1 v7 field | Validity |
| --- | --- | --- | --- |
| `pe_t.state` | `state` | `STATUS state` | `PE_*` mapping from PD17 header |
| `pe_t.caps_epoch` | `caps_epoch` | `GET_CAPS epoch` | not a unique persistent ID |
| `pe_t.caps.pdo[0..count-1]` | `raw_pdos[]` | `GET_CAP n` | expose only if `caps_fresh`; count <= 7 |
| `pe_t.contract_valid` | `contract_valid` | `STATUS v` | **M5-2 adds PE_READY and consistency requirements** |
| `pe_t.pps_active` | `pps_active` | `STATUS pps` | only with a verified contract |
| `pe_t.requested_ma` | `requested_ma` | `STATUS ma` | **requested current**, not measured |
| `pd_sensor_reading_t.ina_current_ua` | `ina_current_ua` | `MEASURE current_mA` | **signed measured current**; conversion happens at wire layer |
| `pd_sensor_reading_t.ina_bus_mv` | `ina_bus_mv` | `MEASURE voltage_mV` | `ina_ok && sensor_fresh && READY` |
| `pd_sensor_reading_t.ina_power_mw` | `ina_power_mw` | not yet exposed | preserved in C; future protocol update |
| `pd_sensor_reading_t.adc_cal_mv` | `adc_bus_mv` | not yet exposed | only `adc_ok && adc_calibrated_range` |
| `pe_t.faults` | `faults` | not yet exposed | future STATUS/DIAG extension |

**Coherence:** While `pe_t` is updated in the main loop, capture must occur after PE state handling in the same loop iteration. Do not call snapshot capture from either USB interrupt. Before using a double-buffer/ISR-based reader, separately design atomic swap/critical sections. Avoid exposing a half-updated PDO array.

**Important difference between M5-1 and M5-2:** M5-1 format can represent a valid contract in transitional states, but this bridge conservatively marks it invalid until PD17 is READY. This is a **draft API policy**, not a measurement of physical contract loss and not a replacement for PD17 policy logic.

**Forward integration:** A future firmware can call `m5_pd_snapshot_capture(&pe, &reading, sample_fresh, &snapshot)` from its main loop. It still needs a transport queue, nonblocking USBFS integration, and an explicit coherent state epoch strategy; none is added here. No PD power is requested by this module.
