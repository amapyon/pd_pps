#ifndef M42_DIAG_H
#define M42_DIAG_H
#include <stdbool.h>
#include <stdint.h>
#include "pd_vbus_history.h"
#include "pd_stop_diag.h"
/* Foreground-only freeze of already-collected ADC history at first PD stop.
   This module NEVER starts ADC conversion, performs I2C, or controls PD. */
#define M42_STOP_SAMPLES 40u
void m42_diag_new_attach(void);
void m42_diag_capture_first_stop(const pd_stop_diag_t *stop);
bool m42_diag_frozen(void);
uint8_t m42_diag_stop_count(void);
bool m42_diag_stop_get(uint8_t index, pd_vbus_point_t *out);
#endif
