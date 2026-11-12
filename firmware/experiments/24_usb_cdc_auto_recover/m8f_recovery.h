#ifndef M8F_RECOVERY_H
#define M8F_RECOVERY_H
#include <stdint.h>

/* SRAM-visible counters. Never used as an accuracy or uptime guarantee. */
#define M8F_RECOVERY_MAGIC 0x4d384652u /* M8FR */
#define M8F_RECOVERY_VERSION 1u
#define M8F_TX_TIMEOUT_MS 2500u

typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile uint32_t bus_resets;
    volatile uint32_t bus_resets_handled;
    volatile uint32_t ep3_submits;
    volatile uint32_t ep3_completes;
    volatile uint32_t tx_timeouts;
    volatile uint32_t soft_reconnects;
    volatile uint32_t last_timeout_age_ticks;
    volatile uint32_t in_flight;
    volatile uint32_t submit_tick;
} M8F_RecoveryHealth;

extern volatile M8F_RecoveryHealth m8f_health;
void M8F_RecoveryInit(void);
void M8F_OnBusResetIRQ(void);
void M8F_OnEp3SubmitBegin(void);
void M8F_OnEp3SubmitResult(int rc);
void M8F_OnEp3CompleteIRQ(void);
/* Return 1 => caller must clear all pending ASCII command/response state. */
int M8F_RecoveryPoll(void);
#endif
