#ifndef PD_SESSION_H
#define PD_SESSION_H
#include <stdint.h>

/* Initialization is split only to preserve the M43B-6 USB IRQ setup order. */
void pd_session_init_core(void);
void pd_session_init_state(void);
/* One foreground iteration; never invoke from an ISR. */
void pd_session_step(uint32_t now_ms);
#endif
