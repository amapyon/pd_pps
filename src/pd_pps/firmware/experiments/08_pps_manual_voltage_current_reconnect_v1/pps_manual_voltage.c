/*
 * 08_pps_manual_voltage_current_reconnect_v1 - CH32X035F8U6 (QFN20)
 *
 * USB PD/PPS manual voltage + requested current with auto reconnect.
 * Debug terminal: WCH-LinkE (PC18/DIO IC pin14, PC19/DCK IC pin16)
 * CC1: PC14 IC pin19; CC2: PC15 IC pin20.
 * Status LED: PB12 IC pin15.
 *
 * Use Ubuntu-side local echo. Do NOT add printf to handle_debug_input().
 * Based on user-tested v3. Preserve v3 PD transport and reconnect logic.
 * Requested current is an RDO limit, not a forced physical load current.
 * Hardware verification for this revision is pending.
 */
#include "ch32fun.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pps_console_command.h"
#include "reconnect_policy.h"

#define USBPD_IMPLEMENTATION
#include "usbpd_current.h"

#define LED_PIN                    PB12
#define LED_ON                     FUN_HIGH
#define LED_OFF                    FUN_LOW
#define INITIAL_VOLTAGE_MV         7000u
#define INITIAL_CURRENT_MA         1000u  /* Requested limit, NOT load current. */
#define PPS_REFRESH_PERIOD_MS      3000u
#define PPS_RESPONSE_TIMEOUT_MS    2000u
#define PPS_SEND_TIMEOUT_MS        1000u
#define NEGOTIATE_TIMEOUT_MS      20000u
#define SOURCE_CAP_REQUEST_FIRST_MS 1200u
#define SOURCE_CAP_REQUEST_INTERVAL_MS 3000u
#define SOURCE_CAP_REQUEST_MAX_ATTEMPTS 4u
#define RETRY_DELAY_MS             1200u
#define CC_DETACH_DEBOUNCE_MS       150u
#define INPUT_LINE_SIZE              32u

static volatile uint32_t s_ms;
static char s_line[INPUT_LINE_SIZE];
static unsigned s_line_length;
static bool s_line_ready;
static bool s_line_overflow;
static bool s_last_was_newline;

/* Same receive-only handler as the previously working original firmware. */
void handle_debug_input(int length, uint8_t *bytes)
{
    for (int i = 0; i < length; ++i) {
        const uint8_t c = bytes[i];
        if (s_line_ready) continue;
        if (c == '\r' || c == '\n') {
            if (s_last_was_newline) continue;
            s_last_was_newline = true;
            s_line[s_line_length] = '\0';
            s_line_ready = true;
            continue;
        }
        s_last_was_newline = false;
        if (c == 8u || c == 127u) {
            if (s_line_length) --s_line_length;
            continue;
        }
        if (s_line_length + 1u < INPUT_LINE_SIZE) {
            s_line[s_line_length++] = (char)c;
        } else {
            s_line_overflow = true;
        }
    }
}

static void systick_init_ms(void);

static void led_init(void)
{
    funGpioInitAll();
    funPinMode(LED_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funDigitalWrite(LED_PIN, LED_OFF);
}

typedef enum {
    MODE_NEGOTIATING,
    MODE_READY,
    MODE_UNSUPPORTED,
    MODE_RETRY_DELAY
} SessionMode;

typedef struct {
    SessionMode mode;
    uint32_t desired_mv;        /* Retained through unplug/replug. */
    uint32_t desired_ma;        /* PPS RDO current request; also retained. */
    uint32_t negotiated_start_ms;
    bool negotiating_started;
    uint32_t last_cap_request_ms;
    uint8_t cap_request_attempts;
    uint32_t last_pps_ms;
    uint32_t retry_start_ms;
    uint8_t pdo_index;
    CCDetachFilter detach;
} Session;

static const char *session_mode_name(SessionMode mode)
{
    switch (mode) {
        case MODE_NEGOTIATING: return "Negotiating";
        case MODE_READY:       return "PPS ready";
        case MODE_UNSUPPORTED: return "No usable PPS APDO";
        case MODE_RETRY_DELAY: return "Retry delay";
        default: return "Unknown";
    }
}

/* Restart software and peripheral PD state without resetting the whole MCU
 * or the SWIO console. In particular, do not leave BMC RX/TX running from the
 * former attachment when resetting the PD state machine.
 */
static void pd_reinitialize(Session *session)
{
    funDigitalWrite(LED_PIN, LED_OFF);
    USBPD_Reset();                 /* Disable USBPD IRQ and clear its state. */
    RCC->AHBPCENR |= RCC_USBPD;    /* Required even on the very first init. */
    USBPD->CONTROL = 0u;           /* Stop BMC RX/TX prior to reconfiguration. */
    const USBPD_Result_e rc = USBPD_Init(eUSBPD_VCC_3V3);
    USBPD_Reset();                 /* Begin in IDLE, USBPD IRQ disabled. */
    cc_detach_filter_clear(&session->detach);
    session->negotiated_start_ms = 0u;
    session->negotiating_started = false;
    session->cap_request_attempts = 0u;
    session->last_cap_request_ms = 0u;
    session->mode = (rc == eUSBPD_OK) ? MODE_NEGOTIATING : MODE_RETRY_DELAY;
    session->retry_start_ms = s_ms;
    session->pdo_index = 0xffu;
    if (rc != eUSBPD_OK)
        printf("ERROR: PD init=%s; will retry\n", USBPD_ResultToStr(rc));
}

static void schedule_retry(Session *session, const char *reason)
{
    printf("PD: %s; retry in %ums (unplug/replug is also supported)\n",
           reason, RETRY_DELAY_MS);
    funDigitalWrite(LED_PIN, LED_OFF);
    USBPD_Reset();
    USBPD->CONTROL = 0u;
    session->mode = MODE_RETRY_DELAY;
    session->retry_start_ms = s_ms;
    session->pdo_index = 0xffu;
    cc_detach_filter_clear(&session->detach);
}

static int choose_pps_pdo(uint32_t mv, uint32_t ma, bool dump)
{
    USBPD_SPR_CapabilitiesMessage_t *caps = NULL;
    const size_t count = USBPD_GetCapabilities(&caps);
    if (!caps || !count) return -1;
    if (dump) printf("PD: Source PDO count=%u\n", (unsigned)count);

    int best = -1;
    uint32_t best_ma = 0u;
    for (size_t i = 0; i < count; ++i) {
        const USBPD_SourcePDO_t *pdo = &caps->Source[i];
        if (dump) {
            /* Preserve the raw 32-bit PDO for diagnosis; never depend on
             * guessed advertisement details from a manufacturer's label. */
            uint32_t raw = 0u;
            memcpy(&raw, pdo, sizeof raw);
            if (pdo->Header.PDOType == eUSBPD_PDO_FIXED) {
                printf("PD: PDO[%u] raw=0x%08lX FIXED %lu mV / %lu mA\n",
                       (unsigned)i, (unsigned long)raw,
                       (unsigned long)pdo->FixedSupply.VoltageIn50mV * 50u,
                       (unsigned long)pdo->FixedSupply.MaxCurrentIn10mA * 10u);
            } else if (pdo->Header.PDOType == eUSBPD_PDO_AUGMENTED &&
                       !USBPD_IsPPS(pdo)) {
                printf("PD: PDO[%u] raw=0x%08lX AUGMENTED subtype=%u (not PPS)\n",
                       (unsigned)i, (unsigned long)raw,
                       (unsigned)pdo->Header.AugmentedType);
            } else if (!USBPD_IsPPS(pdo)) {
                printf("PD: PDO[%u] raw=0x%08lX type=%u (not PPS)\n",
                       (unsigned)i, (unsigned long)raw,
                       (unsigned)pdo->Header.PDOType);
            }
        }
        if (!USBPD_IsPPS(pdo)) continue;
        const uint32_t min_mv = pdo->SPR_PPS.MinVoltageIn100mV * 100u;
        const uint32_t max_mv = pdo->SPR_PPS.MaxVoltageIn100mV * 100u;
        const uint32_t max_ma = pdo->SPR_PPS.MaxCurrentIn50mA * 50u;
        if (dump) {
            uint32_t raw = 0u;
            memcpy(&raw, pdo, sizeof raw);
            printf("PD: PDO[%u] raw=0x%08lX PPS %lu-%lu mV, max %lu mA\n",
                   (unsigned)i, (unsigned long)raw,
                   (unsigned long)min_mv, (unsigned long)max_mv,
                   (unsigned long)max_ma);
        }
        if (mv >= min_mv && mv <= max_mv &&
            max_ma >= ma && max_ma > best_ma) {
            best = (int)i;
            best_ma = max_ma;
        }
    }
    return best;
}

/* A transport timeout is recoverable and must not halt the whole firmware. */
static bool send_and_wait_pps(uint8_t index, uint32_t mv,
                              uint32_t ma, const char *reason)
{
    USBPD_Result_e result = eUSBPD_BUSY;
    uint32_t start = s_ms;
    do {
        result = USBPD_SelectPPS(index, mv, ma);
        if (result != eUSBPD_BUSY) break;
        Delay_Ms(5);
    } while ((uint32_t)(s_ms - start) < PPS_SEND_TIMEOUT_MS);

    const bool quiet_refresh = (strcmp(reason, "refresh") == 0);
    if (!quiet_refresh || result != eUSBPD_OK)
        printf("PPS: %s %lu mV / %lu mA, send=%s, state=%s\n",
            reason, (unsigned long)mv, (unsigned long)ma,
            USBPD_ResultToStr(result), USBPD_StateToStr(USBPD_GetState()));
    if (result != eUSBPD_OK) return false;

    start = s_ms;
    while ((uint32_t)(s_ms - start) < PPS_RESPONSE_TIMEOUT_MS) {
        const USBPD_State_e state = USBPD_GetState();
        if (state == eSTATE_PS_RDY) {
            if (!quiet_refresh)
                printf("PPS: %s PS_RDY at requested %lu mV / %lu mA\n",
                    reason, (unsigned long)mv, (unsigned long)ma);
            return true;
        }
        if (state != eSTATE_WAIT_ACCEPT && state != eSTATE_WAIT_PS_RDY) {
            printf("ERROR: unexpected PD state=%s\n", USBPD_StateToStr(state));
            return false;
        }
        Delay_Ms(10);
    }
    printf("ERROR: PPS response timeout, state=%s\n",
        USBPD_StateToStr(USBPD_GetState()));
    return false;
}

static void print_help(void)
{
    printf("Manual USB PD PPS voltage and requested current limit.\n");
    printf("  7.02        : set voltage 7.02 V (legacy v3 command)\n");
    printf("  7020        : set voltage 7020 mV (legacy v3 command)\n");
    printf("  v 8.00      : set voltage 8.00 V (or v 8000)\n");
    printf("  i 1.50      : request current limit 1.50 A (or i 1500)\n");
    printf("  set 8.00 1.50 : set voltage+current together\n");
    printf("  8000 1500  : same, both values in mV and mA\n");
    printf("  status (s) : targets, session and PD state\n");
    printf("  caps (c)   : last Source PDOs (v3 diagnostics)\n");
    printf("  retry (r)  : restart PD negotiation\n");
    printf("  help (?)   : show this text\n");
    printf("  PPS step: voltage 20mV, requested current 50mA.\n");
    printf("  Voltage: 5000..21000mV; current: 1000..6350mA;\n");
    printf("  BOTH must fit the charger's actual PPS APDO.\n");
    printf("  Both targets survive a cable disconnect/reconnect.\n");
    printf("  KWS-X1 PPS V/A shows the negotiated REQUEST, not measured IBUS.\n");
    printf("  PS_RDY confirms PD negotiation, not voltage/current accuracy.\n");
}

static void print_status(const Session *session)
{
    printf("STATUS: desired=%lu mV, current limit=%lu mA, mode=%s, PD=%s, PDO=%d\n",
        (unsigned long)session->desired_mv, (unsigned long)session->desired_ma,
        session_mode_name(session->mode), USBPD_StateToStr(USBPD_GetState()),
        session->pdo_index == 0xffu ? -1 : (int)session->pdo_index);
    printf("STATUS: PPS requested current is NOT measured load current.\n");
}

static void accept_pps_after_fixed_contract(Session *session)
{
    printf("PD: negotiate=OK, state=%s\n", USBPD_StateToStr(USBPD_GetState()));
    const int index = choose_pps_pdo(session->desired_mv, session->desired_ma, true);
    if (index < 0) {
        printf("WARN: no PPS APDO supports %lumV/%lumA; fixed contract remains.\n",
               (unsigned long)session->desired_mv,
               (unsigned long)session->desired_ma);
        session->mode = MODE_UNSUPPORTED;
        print_status(session);
        printf("VI> ");
        return;
    }
    if (!send_and_wait_pps((uint8_t)index, session->desired_mv,
                           session->desired_ma, "restore")) {
        schedule_retry(session, "initial PPS request failed");
        return;
    }
    session->pdo_index = (uint8_t)index;
    session->mode = MODE_READY;
    session->last_pps_ms = s_ms;
    funDigitalWrite(LED_PIN, LED_ON);
    printf("PD: PPS ready; restored %lu mV / %lu mA request\n",
           (unsigned long)session->desired_mv,
           (unsigned long)session->desired_ma);
    print_status(session);
    printf("VI> ");
}

static void process_command(Session *session)
{
    if (!s_line_ready) return;
    char local[INPUT_LINE_SIZE];
    const bool overflow = s_line_overflow;
    memcpy(local, s_line, sizeof local);
    s_line_length = 0u;
    s_line_overflow = false;
    s_line_ready = false;
    if (overflow) {
        printf("ERROR: command too long\n");
        printf("VI> ");
        return;
    }
    const PPSCommand cmd = pps_parse_command(local);
    switch (cmd.type) {
        case PPS_CMD_EMPTY:
            printf("VI> ");
            return;
        case PPS_CMD_HELP:
            print_help();
            break;
        case PPS_CMD_STATUS:
            print_status(session);
            break;
        case PPS_CMD_CAPS:
            if (USBPD_GetCapabilities(NULL) == 0)
                printf("PD: no Source_Capabilities received yet\n");
            else
                (void)choose_pps_pdo(session->desired_mv,
                                     session->desired_ma, true);
            break;
        case PPS_CMD_RETRY:
            schedule_retry(session, "manual restart");
            break;
        case PPS_CMD_INVALID:
            printf("ERROR: %s\n", cmd.error);
            break;
        case PPS_CMD_SET_V:
        case PPS_CMD_SET_I:
        case PPS_CMD_SET_VI: {
            const uint32_t new_mv = (cmd.type == PPS_CMD_SET_I)
                ? session->desired_mv : cmd.mv;
            const uint32_t new_ma = (cmd.type == PPS_CMD_SET_V)
                ? session->desired_ma : cmd.ma;
            if (!pps_valid_voltage(new_mv)) {
                printf("ERROR: voltage must be 5000..21000mV in 20mV steps\n");
                break;
            }
            if (!pps_valid_current(new_ma)) {
                printf("ERROR: requested current must be 1000..6350mA in 50mA steps\n");
                break;
            }
            if (session->mode == MODE_READY || session->mode == MODE_UNSUPPORTED) {
                const int new_index = choose_pps_pdo(new_mv, new_ma, false);
                if (new_index < 0) {
                    printf("ERROR: no PPS APDO supports %lumV / %lumA; target unchanged\n",
                           (unsigned long)new_mv, (unsigned long)new_ma);
                } else if (new_mv == session->desired_mv &&
                           new_ma == session->desired_ma &&
                           session->mode == MODE_READY &&
                           (uint8_t)new_index == session->pdo_index) {
                    printf("PPS: unchanged (%lumV / %lumA)\n",
                           (unsigned long)new_mv, (unsigned long)new_ma);
                } else {
                    /* Preserve requested V and I after a recoverable failure.
                     * Change both as one PD RDO, never as two intermediate RDOs. */
                    session->desired_mv = new_mv;
                    session->desired_ma = new_ma;
                    funDigitalWrite(LED_PIN, LED_OFF);
                    if (!send_and_wait_pps((uint8_t)new_index, new_mv,
                                           new_ma, "set")) {
                        schedule_retry(session, "PPS change failed");
                    } else {
                        session->pdo_index = (uint8_t)new_index;
                        session->mode = MODE_READY;
                        session->last_pps_ms = s_ms;
                        funDigitalWrite(LED_PIN, LED_ON);
                        print_status(session);
                    }
                }
            } else {
                session->desired_mv = new_mv;
                session->desired_ma = new_ma;
                printf("PPS: saved target %lumV / %lumA; apply after reconnect\n",
                       (unsigned long)new_mv, (unsigned long)new_ma);
            }
            break;
        }
        default:
            printf("ERROR: unknown command\n");
            break;
    }
    printf("VI> ");
}

static void tick_session(Session *session)
{
    const uint32_t now = s_ms;
    if (session->mode == MODE_RETRY_DELAY) {
        if ((uint32_t)(now - session->retry_start_ms) >= RETRY_DELAY_MS) {
            pd_reinitialize(session);
            printf("PD: ready to detect charger\n");
        }
        return;
    }

    /* Only read the active comparator (no CC_SEL writes while RX is live).
     * ActiveCCPresent is not valid in the IDLE state. */
    const USBPD_State_e state = USBPD_GetState();
    if (state != eSTATE_IDLE) {
        if (cc_detach_filter_update(&session->detach,
                USBPD_ActiveCCPresent(), now, CC_DETACH_DEBOUNCE_MS)) {
            printf("PD: cable detached; waiting for reconnect\n");
            pd_reinitialize(session);
            printf("VI> ");
            return;
        }
    } else {
        cc_detach_filter_clear(&session->detach);
    }

    if (session->mode == MODE_NEGOTIATING) {
        const USBPD_Result_e rc = USBPD_SinkNegotiate();
        const USBPD_State_e next_state = USBPD_GetState();
        if (next_state == eSTATE_IDLE) {
            /* Not connected: wait indefinitely, don't count as timeout. */
            session->negotiated_start_ms = 0u;
            session->negotiating_started = false;
            return;
        }
        if (!session->negotiating_started) {
            session->negotiated_start_ms = now;
            session->negotiating_started = true;
            printf("PD: cable detected; negotiating\n");
        }
        if (rc == eUSBPD_OK) {
            accept_pps_after_fixed_contract(session);
        } else if (rc != eUSBPD_BUSY) {
            printf("ERROR: negotiate=%s\n", USBPD_ResultToStr(rc));
            schedule_retry(session, "PD negotiate failed");
        } else if (next_state == eSTATE_CABLE_DETECT &&
                   session->cap_request_attempts < SOURCE_CAP_REQUEST_MAX_ATTEMPTS &&
                   ((session->cap_request_attempts == 0u &&
                     (uint32_t)(now - session->negotiated_start_ms) >= SOURCE_CAP_REQUEST_FIRST_MS) ||
                    (session->cap_request_attempts > 0u &&
                     (uint32_t)(now - session->last_cap_request_ms) >= SOURCE_CAP_REQUEST_INTERVAL_MS))) {
            /* As in Linux TCPM: request capabilities explicitly if a PD
             * source is detected but doesn't send Source_Capabilities.
             * Do not disturb RX when there is an active TX exchange. */
            const USBPD_Result_e cap_rc = USBPD_RequestSourceCapabilities();
            if (cap_rc == eUSBPD_OK) {
                ++session->cap_request_attempts;
                session->last_cap_request_ms = now;
                printf("PD: Get_Source_Cap sent (%u/%u)\n",
                    (unsigned)session->cap_request_attempts,
                    (unsigned)SOURCE_CAP_REQUEST_MAX_ATTEMPTS);
            }
        } else if ((uint32_t)(now - session->negotiated_start_ms) > NEGOTIATE_TIMEOUT_MS) {
            printf("ERROR: PD negotiation timeout, state=%s, cap_requests=%u\n",
                   USBPD_StateToStr(next_state), (unsigned)session->cap_request_attempts);
            schedule_retry(session, "negotiation timeout");
        }
        return;
    }

    if (session->mode == MODE_READY || session->mode == MODE_UNSUPPORTED) {
        /* Receiving a new Source_Capabilities advertisement does not by
         * itself prove that the USB-C plug was detached.  The library
         * deliberately changes state to eSTATE_SOURCE_CAP whenever one
         * arrives, even while an existing fixed/PPS contract is active.
         * Renegotiate without tearing down CC/BMC and causing a reset storm.
         */
        if (state == eSTATE_SOURCE_CAP) {
            printf("PD: updated Source_Capabilities; renegotiating without USBPD_Reset\n");
            session->mode = MODE_NEGOTIATING;
            session->negotiated_start_ms = now;
            session->negotiating_started = true;
            session->cap_request_attempts = 0u;
            session->pdo_index = 0xffu;
            funDigitalWrite(LED_PIN, LED_OFF);
            return;
        }
        if (state != eSTATE_PS_RDY) {
            printf("ERROR: PD contract lost, state=%s\n", USBPD_StateToStr(state));
            schedule_retry(session, "lost PD contract");
            return;
        }
        if (session->mode == MODE_READY &&
                (uint32_t)(now - session->last_pps_ms) >= PPS_REFRESH_PERIOD_MS) {
            if (!send_and_wait_pps(session->pdo_index, session->desired_mv,
                                   session->desired_ma, "refresh")) {
                schedule_retry(session, "PPS refresh failed");
            } else {
                session->last_pps_ms = s_ms;
            }
        }
    }
}

int main(void)
{
    SystemInit();
    led_init();
    systick_init_ms();
    WaitForDebuggerToAttach(0);
    printf("BOOT: CH32X035 PPS manual voltage+current (based on reconnect v3)\n");
    printf("PPS RDO: 20mV voltage, 50mA requested current steps\n");

    Session session = {0};
    session.desired_mv = INITIAL_VOLTAGE_MV;
    session.desired_ma = INITIAL_CURRENT_MA;
    session.pdo_index = 0xffu;
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA;
    pd_reinitialize(&session);
    print_help();
    printf("PD: waiting for charger (plug/unplug supported)\n");
    printf("VI> ");

    for (;;) {
        poll_input();
        process_command(&session);
        tick_session(&session);
        /* Keep CC detection/initial PD negotiation responsive. */
        Delay_Ms(session.mode == MODE_NEGOTIATING ? 1u : 10u);
    }
}

static void systick_init_ms(void)
{
    SysTick->CTLR = 0;
    NVIC_EnableIRQ(SysTick_IRQn);
    uint64_t cnt = SysTick->CNTL | ((uint64_t)SysTick->CNTH << 32);
    cnt += (FUNCONF_SYSTEM_CORE_CLOCK / 1000u) - 1u;
    SysTick->CMPL = (uint32_t)(cnt & 0xFFFFFFFFu);
    SysTick->CMPH = (uint32_t)(cnt >> 32);
    s_ms = 0u;
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE | SYSTICK_CTLR_STCLK;
}

void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
    uint64_t cnt = SysTick->CNTL | ((uint64_t)SysTick->CNTH << 32);
    cnt += (FUNCONF_SYSTEM_CORE_CLOCK / 1000u) - 1u;
    SysTick->CMPL = (uint32_t)(cnt & 0xFFFFFFFFu);
    SysTick->CMPH = (uint32_t)(cnt >> 32);
    SysTick->SR = 0;
    s_ms++;
}
