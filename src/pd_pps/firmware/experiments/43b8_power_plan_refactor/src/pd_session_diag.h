#ifndef PD_SESSION_DIAG_H
#define PD_SESSION_DIAG_H
#include <stdbool.h>
#include <stdint.h>
#include "pd_policy.h"
#include "pd_stability.h"
#include "pd_stop_diag.h"

void pd_diag_init(void);
void pd_diag_on_detach(uint32_t now);
bool pd_diag_report_step(uint32_t now, pe_t *policy,
                         pd_stability_t *stats, pd_stop_diag_t *stop);
void pd_diag_finish_cleanup(void);
void pd_diag_heartbeat(uint32_t now, const pe_t *policy);
#endif
