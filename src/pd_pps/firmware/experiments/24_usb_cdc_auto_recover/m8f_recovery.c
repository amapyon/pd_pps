/* M8F USB CDC recovery, experiment 24. No PD/PPS output control.
 *
 * EP3's busy bit is NEVER force-cleared: that can destroy USB data toggles.
 * After a bounded TX timeout, perform a whole-device disconnect/reinit.
 * A bus reset invalidates buffered protocol state even without a TX timeout.
 *
 * IMPORTANT: This is a prototype with no PC VBUS sense. It cannot detect or
 * prevent physical unplugging, hub power loss or permanent disconnection.
 * Copyright (c) 2026. Released under MIT license (see LICENSE).
 */
#include "m8f_recovery.h"
#ifdef M8F_RECOVERY_HOST_TEST
extern uint32_t M8F_HostTicks(void);
extern void M8F_HostReenumerate(void);
#define M8F_TIME_TICKS_PER_MS 6u /* Host-only 6 MHz SysTick model */
static uint32_t M8F_NowTicks(void) { return M8F_HostTicks(); }
static void M8F_DoReenumeration(void) { M8F_HostReenumerate(); }
#else
#include "ch32fun.h"
#include "fsusb.h"
#define M8F_TIME_TICKS_PER_MS DELAY_MS_TIME
static uint32_t M8F_NowTicks(void) { return (uint32_t)SysTick->CNT; }
static void M8F_DoReenumeration(void)
{
    /* A controlled USB disconnect is safer than forcibly clearing EP3 busy.
     * Only called from main, never an IRQ. The local, pinned fsusb.c copy
     * is used for USBFSSetup; the project-wide library is NOT modified.
     */
    NVIC_DisableIRQ(USB_IRQn);
    USBFS->INT_EN = 0;
    USBFS->UDEV_CTRL = 0; /* disable device port */
    /* Physically remove USB pull-up for a host-visible disconnect.
     * CH32X03x upstream fsusb.c sets pull-ups through AFIO->CTLR. */
    AFIO->CTLR &= ~(UDP_PUE_MASK | UDM_PUE_MASK);
    USBFS->BASE_CTRL = 0;
    Delay_Ms(150u);
    (void)USBFSSetup(); /* reinit endpoints, device address and pull-up */
}
#endif

volatile M8F_RecoveryHealth m8f_health __attribute__((used));
static uint32_t m8f_seen_bus_reset;
void M8F_RecoveryInit(void)
{
    m8f_health.magic = M8F_RECOVERY_MAGIC;
    m8f_health.version = M8F_RECOVERY_VERSION;
    m8f_seen_bus_reset = m8f_health.bus_resets;
}
void M8F_OnBusResetIRQ(void)
{
    /* ISR writer: atomic aligned word stores on RV32. */
    m8f_health.bus_resets++;
    m8f_health.in_flight = 0u;
}
void M8F_OnEp3SubmitBegin(void)
{
    /* Called BEFORE USBFS_SendEndpoint enables ACK for EP3. */
    m8f_health.submit_tick = M8F_NowTicks();
    m8f_health.in_flight = 1u;
}
void M8F_OnEp3SubmitResult(int rc)
{
    if (rc == 0) m8f_health.ep3_submits++;
    else m8f_health.in_flight = 0u;
}
void M8F_OnEp3CompleteIRQ(void)
{
    m8f_health.ep3_completes++;
    m8f_health.in_flight = 0u;
}
int M8F_RecoveryPoll(void)
{
    const uint32_t reset_count = m8f_health.bus_resets;
    if (reset_count != m8f_seen_bus_reset) {
        m8f_seen_bus_reset = reset_count;
        m8f_health.bus_resets_handled++;
        m8f_health.in_flight = 0u;
        return 1; /* Drop stale command, line and reply buffers. */
    }
    if (m8f_health.in_flight != 0u) {
        const uint32_t age = (uint32_t)(M8F_NowTicks() - m8f_health.submit_tick);
        if (age >= (uint32_t)(M8F_TX_TIMEOUT_MS * M8F_TIME_TICKS_PER_MS)) {
            m8f_health.last_timeout_age_ticks = age;
            m8f_health.tx_timeouts++;
            m8f_health.in_flight = 0u;
            M8F_DoReenumeration();
            m8f_health.soft_reconnects++;
            m8f_seen_bus_reset = m8f_health.bus_resets;
            return 1; /* Drop stale transport state; require a NEW request. */
        }
    }
    return 0;
}
