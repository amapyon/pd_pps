/* Pure host tests for the deterministic recovery state machine. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define M8F_RECOVERY_HOST_TEST
#include "../m8f_recovery.c"
static uint32_t ticks;
static unsigned reconnects;
uint32_t M8F_HostTicks(void) { return ticks; }
void M8F_HostReenumerate(void) { reconnects++; }
static void need(int check, const char *why)
{
    if (!check) { fprintf(stderr, "FAIL %s\n", why); exit(1); }
}
int main(void)
{
    M8F_RecoveryInit();
    need(m8f_health.magic == M8F_RECOVERY_MAGIC, "health magic");
    need(M8F_RecoveryPoll() == 0, "healthy idle");
    M8F_OnEp3SubmitBegin();
    M8F_OnEp3SubmitResult(-1);
    ticks += M8F_TX_TIMEOUT_MS * 6u + 1u;
    need(M8F_RecoveryPoll() == 0, "rejected transfer never times out");
    M8F_OnEp3SubmitBegin();
    M8F_OnEp3SubmitResult(0);
    ticks += (M8F_TX_TIMEOUT_MS-1u) * 6u;
    need(M8F_RecoveryPoll() == 0, "no premature recovery");
    M8F_OnEp3CompleteIRQ();
    ticks += 12u;
    need(M8F_RecoveryPoll() == 0, "completed IN never triggers reconnect");
    need(reconnects == 0, "no spurious reconnect");
    M8F_OnEp3SubmitBegin();
    M8F_OnEp3SubmitResult(0);
    ticks += M8F_TX_TIMEOUT_MS * 6u;
    need(M8F_RecoveryPoll() == 1, "TX timeout triggers protocol reset");
    need(reconnects == 1 && m8f_health.tx_timeouts == 1, "full device reenumeration");
    need(M8F_RecoveryPoll() == 0, "no repeated recovery while idle");
    M8F_OnBusResetIRQ();
    need(M8F_RecoveryPoll() == 1, "bus reset invalidates buffered state");
    need(m8f_health.bus_resets_handled == 1, "bus reset counted");
    need(M8F_RecoveryPoll() == 0, "no duplicate reset handling");
    /* Tick-wrap safety: elapsed subtraction is modulo 2^32. */
    ticks = UINT32_MAX - 12u;
    M8F_OnEp3SubmitBegin();
    M8F_OnEp3SubmitResult(0);
    ticks += M8F_TX_TIMEOUT_MS * 6u + 2u;
    need(M8F_RecoveryPoll() == 1, "tick wrap timeout");
    need(reconnects == 2, "second reenumeration");
    puts("PASS: recovery state machine (timeout, reset, completion, wrap)");
    return 0;
}
