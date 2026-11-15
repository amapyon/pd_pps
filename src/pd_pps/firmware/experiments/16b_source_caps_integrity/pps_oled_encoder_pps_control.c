/*
 * M4-B: 16_oled_encoder_pps_control - CH32X035F8U6 (QFN20)
 *
 * USB PD/PPS manual voltage + requested current with auto reconnect.
 * Debug terminal: WCH-LinkE (PC18/DIO IC pin14, PC19/DCK IC pin16)
 * CC1: PC14 IC pin19; CC2: PC15 IC pin20.
 * Status LED: PB12 IC pin15.
 *
 * Use Ubuntu-side local echo. Do NOT add printf to handle_debug_input().
 * Based on user-tested v3. Preserve v3 PD transport and reconnect logic.
 * Requested current is an RDO limit, not a forced physical load current.
 * M4-A has been hardware-tested on this board. M4-B live PPS apply is NEW.
 * Do not claim output voltage verified from a PS_RDY response alone.
 */
#include "ch32fun.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pps_console_command.h"
#include "pps_apdo_limits.h"
#include "reconnect_policy.h"
#include "adc_voltage.h"
#include "adc_calibration.h"
#include "ina226.h"
#include "m2_bus_adapter.h"
#include "m2_dashboard.h"
#include "m3_input.h"
#include "m3_gpio.h"
#include "m4_ui.h"
#include "m4_screen.h"
#include "m4_apply_guard.h"

/* Experimental 7V/1A PPS trial with live RX-length diagnostics.
 * Do not enable UI Apply or variable console voltage/current commands. */
#define M4C_RX_DIAGNOSTIC 1u
#define M4C_PPS_TRIAL_LOCK 1u
/* Main-loop checkpoint probe: sampled by USBPD ISR only when relevant.
 * 0=setup, 1=input, 2=command, 3=PD tick, 4=trace, 5=VBUS diagnostics,
 * 6=sync source, 7=buttons, 8=pending action, 9=OLED, 10=sleep,
 * 21=Soft Reset started, 22=retry printf, 23=retry printf returned.
 * Never print or wait from the interrupt handler. */
static volatile uint8_t s_m4c_loop_stage;
static volatile uint32_t s_m4c_loop_stage_tick;
static inline void m4c_loop_checkpoint(uint8_t stage)
{
    s_m4c_loop_stage_tick = SysTick->CNTL;
    s_m4c_loop_stage = stage;
}
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
/* CC diagnosis can stall fast PD timing via debugprintf; keep sampling but
 * silence the per-edge / periodic CC logs during RX_RESET investigation. */
#define M4C_CC_VERBOSE_DIAG           0u
#define INPUT_LINE_SIZE              32u

/* M2 OLED retained: Set 0 to compile without OLED activity for A/B diagnosis.
 * This does NOT disable INA226, PD control, or serial meter commands. */
#define M2_OLED_ENABLE               1u
#define M2_OLED_POWER_SETTLE_MS     150u
#define M2_INA_SAMPLE_PERIOD_MS     800u

/* M4 stage B: live UI change enabled. The explicit CONFIRM page is required.
 * Guard live PD state, unchanged original setting, same APDO and valid V/I.
 * External load must remain disconnected for the first hardware trials. */
#define M4_PPS_APPLY_ENABLE         0u

static volatile uint32_t s_ms;
static char s_line[INPUT_LINE_SIZE];
static unsigned s_line_length;
static bool s_line_ready;
static bool s_line_overflow;
static bool s_last_was_newline;

#if M2_OLED_ENABLE
/* Persistent buffer: 1024 bytes framebuffer, never allocate on stack. */
static m2_dashboard_t s_m2_display;
static bool s_m2_probe_done;
static bool s_m2_oled_ok;
static bool s_m2_fault_reported;
static uint32_t s_m2_last_sample_ms;

static bool m2_start(void *context)
{
    (void)context;
    return ina226_i2c_begin();
}

static bool m2_send(uint8_t data, void *context)
{
    (void)context;
    return ina226_i2c_send(data);
}

static void m2_end(void *context)
{
    (void)context;
    ina226_i2c_end();
}

static m2_i2c_ops_t s_m2_bus = {m2_start, m2_send, m2_end, NULL};
#endif

/* M3: GPIO sampling only; it must NEVER change the PD request. */
static m3_input_t s_m3_input;
static uint32_t s_m3_last_poll_ms;
static m4_ui_t s_m4_ui;
static m4_action_t s_m4_pending;
static uint32_t s_m4_last_caps_ms;
static bool s_m4_caps_sampled;
#if M2_OLED_ENABLE
static bool s_m4_screen_rendered;
static uint32_t s_m4_last_render_ms;
static uint32_t s_m4_last_render_revision;
#endif

static void m3_service(void)
{
    const uint32_t now = s_ms;
    /* Poll at most once per 2 ms. No interrupt handler and no delays here. */
    if ((uint32_t)(now - s_m3_last_poll_ms) < M3_POLL_PERIOD_MS) return;
    s_m3_last_poll_ms = now;
    const m3_levels_t pins = m3_gpio_read();
    const m3_events_t ev = m3_input_sample(&s_m3_input, pins, now);
    if (ev.rotation > 0)
        printf("M3: EC11 CW   pos=%ld total=%lu\n",
               (long)s_m3_input.position, (unsigned long)s_m3_input.cw_count);
    else if (ev.rotation < 0)
        printf("M3: EC11 CCW  pos=%ld total=%lu\n",
               (long)s_m3_input.position, (unsigned long)s_m3_input.ccw_count);
    if (ev.encoder_press)
        printf("M3: EC11 PUSH count=%lu\n", (unsigned long)s_m3_input.press_count);
    if (ev.back_press)
        printf("M3: BACK PUSH count=%lu\n", (unsigned long)s_m3_input.back_count);
    if (ev.rotation || ev.encoder_press || ev.back_press) {
        const m4_action_t action = m4_ui_input(&s_m4_ui, ev);
        if (action.apply) s_m4_pending = action;
    }
}

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
    MODE_RETRY_DELAY,
    MODE_SOFT_RESET_STALLED /* require physical cable detach to recover */
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
    bool soft_reset_attempted;
    CCDetachFilter detach;
} Session;

#if M2_OLED_ENABLE
/* Called only from the main loop, after PD and console processing.
 * PD negotiation is prioritized; only one OLED I2C transaction per tick.
 */
static void m2_service(const Session *session)
{
    const uint32_t now = s_ms;
    const bool active_negotiation =
        session->mode == MODE_NEGOTIATING && session->negotiating_started;

    if (!s_m2_probe_done && now >= M2_OLED_POWER_SETTLE_MS &&
        !active_negotiation) {
        s_m2_probe_done = true;
        s_m2_oled_ok = m2_dashboard_init(&s_m2_display, m2_bus_write,
                                          &s_m2_bus);
        if (s_m2_oled_ok) {
            printf("M2: SSD1306 ready at 0x%02X (PA2 SCL / PA3 SDA)\n",
                   (unsigned)s_m2_display.oled.address7);
        } else {
            printf("M2: no SSD1306 at 0x3C/0x3D (PD/INA226 continue)\n");
        }
    }
    if (!s_m2_oled_ok) return;

    m2_dashboard_set_pd(&s_m2_display, (int32_t)session->desired_mv,
                        (uint16_t)session->desired_ma,
                        session->mode == MODE_READY &&
                        USBPD_GetState() == eSTATE_PS_RDY);

    /* The original 'm' and 'status' commands remain untouched; display
     * measurements are sampled independently from the same read function.
     * No INA226 accesses in an IRQ or while actively negotiating PD.
     */
    if (!active_negotiation &&
        (uint32_t)(now - s_m2_last_sample_ms) >= M2_INA_SAMPLE_PERIOD_MS) {
        INA226Reading r;
        const bool valid = ina226_read(&r);
        s_m2_last_sample_ms = now;
        m2_dashboard_set_ina_raw(&s_m2_display,
                                  valid ? r.bus_raw : 0u,
                                  valid ? r.shunt_raw : 0,
                                  valid);
    }
    if (active_negotiation) return;
    /* M4 owns framebuffer rendering; M2 still owns INA data + SSD1306 I2C.
     * Never modify framebuffer during an in-progress split OLED transfer. */
    if (s_m2_display.oled.busy) {
        if (m2_ssd1306_flush_step(&s_m2_display.oled) == M2_FLUSH_ERROR)
            s_m2_display.display_fault = true;
    } else if (!s_m2_display.display_fault &&
               (!s_m4_screen_rendered ||
                s_m4_last_render_revision != s_m4_ui.revision ||
                (uint32_t)(now - s_m4_last_render_ms) >=
                  (s_m4_ui.page == M4_PAGE_MONITOR ? 1000u : 500u))) {
        m4_screen_draw(&s_m2_display.oled, &s_m4_ui,
                       &s_m2_display.values, M4_PPS_APPLY_ENABLE != 0u);
        if (!m2_ssd1306_begin_flush(&s_m2_display.oled) ||
            m2_ssd1306_flush_step(&s_m2_display.oled) == M2_FLUSH_ERROR)
            s_m2_display.display_fault = true;
        s_m4_screen_rendered = true;
        s_m4_last_render_ms = now;
        s_m4_last_render_revision = s_m4_ui.revision;
    }
    if (s_m2_display.display_fault && !s_m2_fault_reported) {
        s_m2_fault_reported = true;
        printf("M2: OLED transfer error; display stopped (PD/INA226 continue)\n");
    }
}
#endif

static const char *session_mode_name(SessionMode mode)
{
    switch (mode) {
        case MODE_NEGOTIATING: return "Negotiating";
        case MODE_READY:       return "PPS ready";
        case MODE_UNSUPPORTED: return "No usable PPS APDO";
        case MODE_RETRY_DELAY: return "Retry delay";
        case MODE_SOFT_RESET_STALLED: return "Soft Reset failed: unplug C1";
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
    session->soft_reset_attempted = false;
    if (rc != eUSBPD_OK)
        printf("ERROR: PD init=%s; will retry\n", USBPD_ResultToStr(rc));
}

static void schedule_retry(Session *session, const char *reason)
{
    printf("PD: %s; retry in %ums (unplug/replug is also supported)\n",
           reason, RETRY_DELAY_MS);
#if M4C_RX_DIAGNOSTIC
    printf("M4C TRACE: retry_requested state=%u rx_events=%lu\n",
           (unsigned)USBPD_GetState(),
           (unsigned long)s_m4c_rx_trace_written);
#endif
    funDigitalWrite(LED_PIN, LED_OFF);
    USBPD_Reset();
    USBPD->CONTROL = 0u;
    session->mode = MODE_RETRY_DELAY;
    session->retry_start_ms = s_ms;
    session->pdo_index = 0xffu;
    session->soft_reset_attempted = false;
    cc_detach_filter_clear(&session->detach);
}

static int choose_pps_pdo(uint32_t mv, uint32_t ma, bool dump)
{
    USBPD_SPR_CapabilitiesMessage_t caps_snapshot;
    const size_t count = USBPD_CopyCapabilities(&caps_snapshot);
    USBPD_SPR_CapabilitiesMessage_t *caps = count ? &caps_snapshot : NULL;
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
        if (pps_apdo_accepts((PPSApdoLimits){min_mv, max_mv, max_ma}, mv, ma) &&
            max_ma > best_ma) {
            best = (int)i;
            best_ma = max_ma;
        }
    }
    return best;
}

/* Print the limits advertised by the CURRENTLY CONNECTED SOURCE.
 * This is not a single rectangular range when several APDOs exist.
 * Each PPS V/I pair must fit at least one APDO, not any fixed PDO.
 */
static void print_pps_limits(void)
{
    USBPD_SPR_CapabilitiesMessage_t caps_snapshot;
    const size_t count = USBPD_CopyCapabilities(&caps_snapshot);
    USBPD_SPR_CapabilitiesMessage_t *caps = count ? &caps_snapshot : NULL;
    if (!caps || !count) {
        printf("PPS LIMITS: unknown (no Source_Capabilities yet)\n");
        return;
    }
    unsigned found = 0u;
    for (size_t i = 0u; i < count; ++i) {
        const USBPD_SourcePDO_t *const pdo = &caps->Source[i];
        if (!USBPD_IsPPS(pdo)) continue;
        const PPSApdoLimits apdo = {
            .min_mv = pdo->SPR_PPS.MinVoltageIn100mV * 100u,
            .max_mv = pdo->SPR_PPS.MaxVoltageIn100mV * 100u,
            .max_ma = pdo->SPR_PPS.MaxCurrentIn50mA * 50u,
        };
        printf("PPS LIMITS: PDO[%u] %lu-%lu mV; max %lu mA; "
               "power-limited=%s (steps: 20mV/50mA)\n",
               (unsigned)i, (unsigned long)apdo.min_mv,
               (unsigned long)apdo.max_mv, (unsigned long)apdo.max_ma,
               pdo->SPR_PPS.PPSpowerLimited ? "YES" : "NO");
        ++found;
    }
    if (found == 0u)
        printf("PPS LIMITS: no PPS APDO (fixed PDOs are not adjustable PPS).\n");
    else
        printf("PPS LIMITS: V/I pair must fit ONE PPS APDO; "
               "RDO min current encoding=50mA (NOT advertised).\n");
}

/* A transport timeout is recoverable and must not halt the whole firmware. */
static bool send_and_wait_pps(uint8_t index, uint32_t mv,
                              uint32_t ma, const char *reason)
{
#if M4C_PPS_TRIAL_LOCK
    /* Safety gate for *every* PPS send path: startup, reconnect, refresh.
     * The source must currently advertise an APDO supporting 7V/1A.
     * This is not a substitute for a DMM measurement or load protection. */
    if (mv != INITIAL_VOLTAGE_MV || ma != INITIAL_CURRENT_MA ||
        USBPD_GetState() != eSTATE_PS_RDY ||
        !USBPD_ActiveCCPresent() ||
        choose_pps_pdo(mv, ma, false) != (int)index) {
        printf("M4C TRIAL: PPS TX BLOCKED (index=%u %lu mV/%lu mA)\n",
               (unsigned)index, (unsigned long)mv, (unsigned long)ma);
        return false;
    }
#endif

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
#if M4C_PPS_TRIAL_LOCK
    printf("M4C PPS TRIAL: 7000mV / 1000mA only; voltage/current editing disabled.\n");
#else
    printf("Manual USB PD PPS voltage and requested current limit.\n");
    printf("  7.02        : set voltage 7.02 V (legacy v3 command)\n");
    printf("  7020        : set voltage 7020 mV (legacy v3 command)\n");
    printf("  v 8.00      : set voltage 8.00 V (or v 8000)\n");
    printf("  i 1.50      : request current limit 1.50 A (or i 1500)\n");
    printf("  set 8.00 1.50 : set voltage+current together\n");
    printf("  8000 1500  : same, both values in mV and mA\n");
#endif
    printf("  limits (l) : current charger's PPS APDO limits\n");
    printf("  status (s) : targets, PD state, ADC VBUS and INA226 meter\n");
    printf("  meter (m)  : INA226 measured load V/I/P (R010 shunt)\n");
    printf("  scan       : INA226 I2C ACK/ID/CONFIG diagnostics (PA2/PA3)\n");
    printf("  caps (c)   : last Source PDOs (v3 diagnostics)\n");
    printf("  retry (r)  : restart PD negotiation\n");
    printf("  help (?)   : show this text\n");
    printf("  M3 GPIO: PA4=A, PA5=B, PA6=EC11 push, PA7=BACK (all to GND)\n");
    printf("  M4 keys: PUSH=menu/select, BACK=cancel, ROTATE=move/edit.\n");
    printf("  PPS step: voltage 20mV, requested current 50mA.\n");
    printf("  Requests must fit the CURRENT charger's reported PPS APDO.\n");
    printf("  Encoding: 3300..21000mV, 50..6350mA; APDO may be narrower.\n");
    printf("  NOTE: current below the board's actual load may reset the source.\n");
    printf("  Both targets survive a cable disconnect/reconnect.\n");
    printf("  KWS-X1 PPS V/A shows the negotiated REQUEST, not measured IBUS.\n");
    printf("  PS_RDY confirms PD negotiation, not voltage/current accuracy.\n");
}

/* Sensor reads occur ONLY on user command/status, not in PD IRQ or
 * time-critical PPS refresh.  VBUS is measured on the LOAD side of R010.
 */
static void print_ina226(void)
{
    INA226Reading r;
    if (!ina226_read(&r)) {
        printf("INA226: NOT FOUND/read failed; check 3.3V, PA2 SCL, PA3 SDA, GND, pullups\n");
        return;
    }
    const int64_t ua = r.current_ua;
    const uint32_t abs_ua = (uint32_t)(ua < 0 ? -ua : ua);
    const int64_t pmw = r.power_mw;
    const uint32_t abs_mw = (uint32_t)(pmw < 0 ? -pmw : pmw);
    printf("INA226: addr=0x%02X BUS=%lu.%03lu V, I=%s%lu.%03lu mA, P=%s%lu.%03lu W\n",
           (unsigned)r.address,
           (unsigned long)(r.bus_mv / 1000u), (unsigned long)(r.bus_mv % 1000u),
           ua < 0 ? "-" : "", (unsigned long)(abs_ua / 1000u),
           (unsigned long)(abs_ua % 1000u),
           pmw < 0 ? "-" : "", (unsigned long)(abs_mw / 1000u),
           (unsigned long)(abs_mw % 1000u));
    printf("INA226: shunt_raw=%d bus_raw=%u (R010 0.010 ohm; measured load current)\n",
           (int)r.shunt_raw, (unsigned)r.bus_raw);
}

static void print_status(const Session *session)
{
    printf("STATUS: desired=%lu mV, current limit=%lu mA, mode=%s, PD=%s, PDO=%d\n",
        (unsigned long)session->desired_mv, (unsigned long)session->desired_ma,
        session_mode_name(session->mode), USBPD_StateToStr(USBPD_GetState()),
        session->pdo_index == 0xffu ? -1 : (int)session->pdo_index);
    printf("STATUS: PPS requested current is NOT measured load current.\n");
    {
        const uint16_t raw = adc_voltage_read_average(32u);
        if (raw == 0xffffu) {
            printf("STATUS: ADC conversion TIMEOUT\n");
        } else {
            const uint32_t pa0_mv = adc_voltage_input_mv(raw);
            const uint32_t uncal_mv = adc_voltage_vbus_mv(raw);
            const uint32_t cal_mv = adc_calibration_vbus_mv(raw);
            printf("STATUS: ADC_IN0 PA0(pin 2) raw=%u, ADC~%lu mV\n",
                   (unsigned)raw, (unsigned long)pa0_mv);
            printf("STATUS: VBUS uncalibrated=%lu.%03lu V, calibrated=%lu.%03lu V\n",
                   (unsigned long)(uncal_mv / 1000u),
                   (unsigned long)(uncal_mv % 1000u),
                   (unsigned long)(cal_mv / 1000u),
                   (unsigned long)(cal_mv % 1000u));
            if (!adc_calibration_in_range(raw)) {
                printf("STATUS: WARNING calibration outside verified range (raw 457..1012; DMM 4.00..9.04V).\n");
            }
            if (session->mode == MODE_READY && USBPD_GetState() == eSTATE_PS_RDY) {
                const int32_t error_uncal_mv = (int32_t)uncal_mv - (int32_t)session->desired_mv;
                const int32_t error_cal_mv = (int32_t)cal_mv - (int32_t)session->desired_mv;
                printf("STATUS: VBUS minus requested target: uncalibrated=%ld mV, calibrated=%ld mV\n",
                       (long)error_uncal_mv, (long)error_cal_mv);
            } else {
                printf("STATUS: No active confirmed PPS state; requested-target comparison omitted.\n");
            }
        }
    }

    print_ina226();
    print_pps_limits();
}

/* Diagnostic only. PS_RDY is a protocol event, not a voltage measurement.
 * Sample the load-side INA226 outside interrupts. A short settling grace and
 * two low samples prevent reporting normal PPS voltage transitions as faults.
 * Do not change PD state, issue requests, or reset the charger here. */
static void m4c_vbus_diag_service(const Session *session)
{
    static uint32_t last_sample_ms;
    static uint32_t ready_since_ms;
    static bool was_ready;
    static bool low_reported;
    static uint8_t low_samples;
    const uint32_t now = s_ms;
    const bool ready = (session->mode == MODE_READY ||
                        session->mode == MODE_UNSUPPORTED) &&
                       USBPD_GetState() == eSTATE_PS_RDY;
    if (!ready) {
        was_ready = false;
        low_reported = false;
        low_samples = 0u;
        return;
    }
    if (!was_ready) {
        was_ready = true;
        ready_since_ms = now;
        low_samples = 0u;
        low_reported = false;
    }
    if ((uint32_t)(now - ready_since_ms) < 1000u ||
        (uint32_t)(now - last_sample_ms) < 500u) return;
    last_sample_ms = now;
    INA226Reading reading;
    if (!ina226_read(&reading)) {
        if (!low_reported)
            printf("M4C VBUS DIAG: INA226 read failed at t=%lu ms\n",
                   (unsigned long)now);
        return;
    }
    if (reading.bus_mv >= 4000u) {
        low_samples = 0u;
        if (low_reported) {
            printf("M4C VBUS DIAG: RECOVERED t=%lu ms bus=%lu mV state=%u\n",
                   (unsigned long)now, (unsigned long)reading.bus_mv,
                   (unsigned)USBPD_GetState());
            low_reported = false;
        }
        return;
    }
    if (low_samples < 2u) ++low_samples;
    if (low_samples < 2u || low_reported) return;
    low_reported = true;
    const uint16_t adc_raw = adc_voltage_read_average(8u);
    printf("M4C VBUS DIAG: LOW t=%lu ms INA=%lu mV adc_raw=%u "
           "session=%u pd_state=%u cc=%u trace_written=%lu\n",
           (unsigned long)now, (unsigned long)reading.bus_mv,
           (unsigned)adc_raw, (unsigned)session->mode,
           (unsigned)USBPD_GetState(),
           (unsigned)USBPD_ActiveCCPresent(),
           (unsigned long)s_m4c_rx_trace_written);
    printf("M4C VBUS DIAG: regs STATUS=0x%04X CONTROL=0x%04X "
           "CONFIG=0x%04X BMC_BYTE_CNT=%u; PS_RDY != VBUS_OK\n",
           (unsigned)USBPD->STATUS, (unsigned)USBPD->CONTROL,
           (unsigned)USBPD->CONFIG, (unsigned)USBPD->BMC_BYTE_CNT);
}

static void accept_pps_after_fixed_contract(Session *session)
{
    printf("PD: negotiate=OK, state=%s\n", USBPD_StateToStr(USBPD_GetState()));
#if M4C_PPS_TRIAL_LOCK
    session->desired_mv = INITIAL_VOLTAGE_MV;
    session->desired_ma = INITIAL_CURRENT_MA;
    printf("M4C TRIAL: fixed PDO[0] then PPS 7000mV/1000mA only\n");
#endif
    const int index = choose_pps_pdo(session->desired_mv, session->desired_ma, true);
    print_pps_limits();
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
        case PPS_CMD_METER:
            print_ina226();
            break;
        case PPS_CMD_SCAN:
            ina226_scan();
            break;
        case PPS_CMD_LIMITS:
            print_pps_limits();
            break;
        case PPS_CMD_CAPS:
            if (USBPD_GetCapabilities(NULL) == 0)
                printf("PD: no Source_Capabilities received yet\n");
            else
                (void)choose_pps_pdo(session->desired_mv,
                                     session->desired_ma, true);
            break;
        case PPS_CMD_RETRY:
            if ((session->mode == MODE_READY || session->mode == MODE_UNSUPPORTED) &&
                USBPD_GetState() == eSTATE_PS_RDY) {
                const USBPD_Result_e soft_rc = USBPD_BeginSoftReset();
                if (soft_rc != eUSBPD_OK) {
                    printf("PD: Soft_Reset busy; no restart performed (retry later)\n");
                    break;
                }
                /* Experiment: request the advertised PPS APDO directly after
                 * manual Soft_Reset, omitting the intermediate fixed 5V RDO. */
                USBPD_ArmDirectPPSAfterSoftReset(INITIAL_VOLTAGE_MV,
                                                INITIAL_CURRENT_MA);
                /* Keep CC and the BMC receiver active. Soft Reset re-syncs
                 * protocol state with the source before any new Request. */
                session->mode = MODE_NEGOTIATING;
                session->negotiated_start_ms = s_ms;
                session->negotiating_started = true;
                session->cap_request_attempts = 0u;
                session->last_cap_request_ms = 0u;
                session->pdo_index = 0xffu;
                session->soft_reset_attempted = true;
                cc_detach_filter_clear(&session->detach);
                m4c_loop_checkpoint(21u);
                m4c_loop_checkpoint(22u);
                printf("PD: manual retry via SOP Soft_Reset; awaiting Accept\n");
                m4c_loop_checkpoint(23u);
            } else {
                printf("PD: no active contract; using original local retry\n");
                schedule_retry(session, "manual restart");
            }
            break;
        case PPS_CMD_INVALID:
            printf("ERROR: %s\n", cmd.error);
            break;
        case PPS_CMD_SET_V:
        case PPS_CMD_SET_I:
        case PPS_CMD_SET_VI: {
#if M4C_PPS_TRIAL_LOCK
            printf("M4C TRIAL: command ignored; PPS target is locked at 7000mV/1000mA\n");
            break;
#else
            const uint32_t new_mv = (cmd.type == PPS_CMD_SET_I)
                ? session->desired_mv : cmd.mv;
            const uint32_t new_ma = (cmd.type == PPS_CMD_SET_V)
                ? session->desired_ma : cmd.ma;
            if (!pps_valid_voltage(new_mv)) {
                printf("ERROR: PPS voltage must be 3300..21000mV in 20mV steps\n");
                break;
            }
            if (!pps_valid_current(new_ma)) {
                printf("ERROR: PPS requested current must be 50..6350mA in 50mA steps\n");
                break;
            }
            if (session->mode == MODE_READY || session->mode == MODE_UNSUPPORTED) {
                const int new_index = choose_pps_pdo(new_mv, new_ma, false);
                if (new_index < 0) {
                    printf("ERROR: no PPS APDO supports %lumV / %lumA; target unchanged\n",
                           (unsigned long)new_mv, (unsigned long)new_ma);
                    print_pps_limits();
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
                    if (new_ma < 200u)
                        printf("WARN: requested current below 200mA may be less than the "
                               "board + tester load; supply may reset\n");
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
                printf("PPS: saved target %lumV / %lumA; validate against APDO "
                       "and apply after negotiation\n",
                       (unsigned long)new_mv, (unsigned long)new_ma);
            }
            break;
#endif
        }
        default:
            printf("ERROR: unknown command\n");
            break;
    }
    printf("VI> ");
}

/* The active CC comparator is read only once per tick.  This is a
 * diagnostic of the comparator sampled by the detach filter, NOT a raw
 * voltage measurement.  CC selection/configuration is never modified. */
typedef struct {
    uint32_t start_ms;
    uint32_t absent_start_ms;
    uint32_t absent_total_ms;
    uint32_t absent_max_ms;
    uint32_t absent_edges;
    uint32_t restored_edges;
    uint8_t detailed_edges;
    uint32_t last_sample_ms;
    uint32_t max_sample_gap_ms;
    uint32_t max_filter_age_ms;
    uint32_t gaps_over_debounce;
    uint32_t restored_while_pending;
    bool valid;
    bool last_present;
    bool hard_reset_seen;
} M4C_CcStats;
static M4C_CcStats s_cc_stats;

static void m4c_cc_diag_sample(const char *event, const Session *session,
                               USBPD_State_e state, bool present,
                               uint8_t selected_line, uint16_t sampled_cc)
{
    printf("M4C CC SAMPLE: t=%lu event=%s state=%u session=%u "
           "present=%u selected_line=%u sampled_cc=0x%04X comparator_bit=%u "
           "cc_sel=%u status=0x%04X pending=%u absent_ms=%lu\n",
           (unsigned long)s_ms, event, (unsigned)state,
           (unsigned)session->mode, (unsigned)present,
           (unsigned)selected_line, (unsigned)sampled_cc,
           (unsigned)(sampled_cc & PA_CC_AI),
           (unsigned)((USBPD->CONFIG & CC_SEL) != 0),
           (unsigned)(uint16_t)USBPD->STATUS,
           (unsigned)session->detach.absence_pending,
           (unsigned long)(session->detach.absence_pending ?
               (uint32_t)(s_ms - session->detach.absent_since_ms) : 0u));
}

static void m4c_cc_diag_summary(uint32_t now, const Session *session,
                                USBPD_State_e state, uint8_t line,
                                uint16_t sampled_cc, bool present)
{
    printf("M4C CC SUMMARY: t=%lu state=%u session=%u line=%u "
           "present=%u sampled_cc=0x%04X absent_edges=%lu "
           "restored_edges=%lu max_absent_ms=%lu pending=%u "
           "max_sample_gap_ms=%lu gap_ge_150ms=%lu "
           "max_filter_age_ms=%lu restored_pending=%lu\n",
           (unsigned long)now, (unsigned)state, (unsigned)session->mode,
           (unsigned)line, (unsigned)present, (unsigned)sampled_cc,
           (unsigned long)s_cc_stats.absent_edges,
           (unsigned long)s_cc_stats.restored_edges,
           (unsigned long)s_cc_stats.absent_max_ms,
           (unsigned)session->detach.absence_pending,
           (unsigned long)s_cc_stats.max_sample_gap_ms,
           (unsigned long)s_cc_stats.gaps_over_debounce,
           (unsigned long)s_cc_stats.max_filter_age_ms,
           (unsigned long)s_cc_stats.restored_while_pending);
    s_cc_stats.absent_edges = 0u;
    s_cc_stats.restored_edges = 0u;
    s_cc_stats.absent_max_ms = 0u;
    s_cc_stats.max_sample_gap_ms = 0u;
    s_cc_stats.gaps_over_debounce = 0u;
    s_cc_stats.max_filter_age_ms = 0u;
    s_cc_stats.restored_while_pending = 0u;
    s_cc_stats.start_ms = now;
}

static void tick_session(Session *session)
{
    USBPD_M4CProbeMainTick((uint8_t)session->mode);
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
        uint8_t cc_line = 0u;
        uint16_t cc_raw = 0u;
        const bool active_cc = USBPD_ActiveCCSample(&cc_line, &cc_raw);
        /* Snapshot BEFORE changing the filter; a rising CC comparator
         * ends an absence window, so pending/age must be sampled first.
         * The filter must run BEFORE printf: slow debug output can otherwise
         * stretch a 2ms poll into hundreds of milliseconds. */
        const bool was_valid = s_cc_stats.valid;
        const bool edge = was_valid && active_cc != s_cc_stats.last_present;
        const uint32_t sample_gap = was_valid ? now - s_cc_stats.last_sample_ms : 0u;
        const bool pending_before = session->detach.absence_pending;
        const uint32_t filter_age_before = pending_before ?
            now - session->detach.absent_since_ms : 0u;
        const bool detach_confirmed = cc_detach_filter_update(&session->detach,
            active_cc, now, CC_DETACH_DEBOUNCE_MS);
        const bool pending_after = session->detach.absence_pending;
        const uint32_t filter_age_after = pending_after ?
            now - session->detach.absent_since_ms : 0u;
        if (!was_valid) {
            s_cc_stats.valid = true;
            s_cc_stats.last_present = active_cc;
            s_cc_stats.start_ms = now;
        } else {
            if (sample_gap > s_cc_stats.max_sample_gap_ms)
                s_cc_stats.max_sample_gap_ms = sample_gap;
            if (sample_gap >= CC_DETACH_DEBOUNCE_MS)
                ++s_cc_stats.gaps_over_debounce;
            if (filter_age_before > s_cc_stats.max_filter_age_ms)
                s_cc_stats.max_filter_age_ms = filter_age_before;
            if (edge) {
                if (active_cc) {
                    const uint32_t duration = now - s_cc_stats.absent_start_ms;
                    ++s_cc_stats.restored_edges;
                    if (duration > s_cc_stats.absent_max_ms)
                        s_cc_stats.absent_max_ms = duration;
                    if (pending_before)
                        ++s_cc_stats.restored_while_pending;
                } else {
                    ++s_cc_stats.absent_edges;
                    s_cc_stats.absent_start_ms = now;
                }
            }
            s_cc_stats.last_present = active_cc;
        }
        s_cc_stats.last_sample_ms = now;

        /* The first four edges carry timing evidence without flooding UART.
         * Filter state below reflects the actual debounce call, not a state
         * read taken before that call (as in the preceding diagnostics). */
        if (M4C_CC_VERBOSE_DIAG &&
            (!was_valid || (edge && s_cc_stats.detailed_edges < 4u))) {
            m4c_cc_diag_sample(!was_valid ? "attach_sample" :
                               (active_cc ? "cc_restored" : "cc_absent"),
                               session, state, active_cc, cc_line, cc_raw);
            printf("M4C CC FILTER: t=%lu gap_ms=%lu edge=%u "
                   "pending_before=%u age_before_ms=%lu "
                   "pending_after=%u age_after_ms=%lu "
                   "debounce_ms=%u confirmed=%u\n",
                   (unsigned long)now, (unsigned long)sample_gap,
                   (unsigned)edge, (unsigned)pending_before,
                   (unsigned long)filter_age_before, (unsigned)pending_after,
                   (unsigned long)filter_age_after,
                   (unsigned)CC_DETACH_DEBOUNCE_MS,
                   (unsigned)detach_confirmed);
            if (edge)
                ++s_cc_stats.detailed_edges;
        }
        if (M4C_CC_VERBOSE_DIAG &&
            state == eSTATE_HARD_RESET_DETECTED &&
            !s_cc_stats.hard_reset_seen) {
            m4c_cc_diag_sample("hard_reset_latched", session, state,
                               active_cc, cc_line, cc_raw);
            s_cc_stats.hard_reset_seen = true;
        }
        if ((uint32_t)(now - s_cc_stats.start_ms) >= 5000u) {
            if (M4C_CC_VERBOSE_DIAG)
                m4c_cc_diag_summary(now, session, state, cc_line, cc_raw, active_cc);
            else {
                /* Bound counters even when their reports are disabled. */
                s_cc_stats.absent_edges = 0u;
                s_cc_stats.restored_edges = 0u;
                s_cc_stats.absent_max_ms = 0u;
                s_cc_stats.max_sample_gap_ms = 0u;
                s_cc_stats.gaps_over_debounce = 0u;
                s_cc_stats.max_filter_age_ms = 0u;
                s_cc_stats.restored_while_pending = 0u;
                s_cc_stats.start_ms = now;
            }
        }
        if (detach_confirmed) {
            printf("M4C CC FILTER: DETACH t=%lu gap_ms=%lu "
                   "filter_age_ms=%lu sampled_cc=0x%04X state=%u\n",
                   (unsigned long)now, (unsigned long)sample_gap,
                   (unsigned long)filter_age_before,
                   (unsigned)cc_raw, (unsigned)state);
            m4c_cc_diag_sample("detach_debounced", session, state,
                               active_cc, cc_line, cc_raw);
            printf("PD: cable detached; waiting for reconnect\n");
            pd_reinitialize(session);
            memset(&s_cc_stats, 0, sizeof(s_cc_stats));
            printf("VI> ");
            return;
        }
    } else {
        memset(&s_cc_stats, 0, sizeof(s_cc_stats));
        cc_detach_filter_clear(&session->detach);
    }

    if (USBPD_GetState() == eSTATE_HARD_RESET_DETECTED) {
        /* Do not treat Hard Reset as successful Soft Reset or send new
         * Requests with VBUS potentially at vSafe0V. Physical detach
         * performs the normal peripheral reinitialization. */
        if (session->mode != MODE_SOFT_RESET_STALLED) {
            printf("M4C PD: HARD_RESET detected; previous contract invalid; "
                   "Request blocked until unplug/replug\n");
            print_ina226();
            funDigitalWrite(LED_PIN, LED_OFF);
            session->pdo_index = 0xffu;
            session->mode = MODE_SOFT_RESET_STALLED;
        }
        return;
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
        if (next_state == eSTATE_SOFT_RESET_FAILED) {
            printf("ERROR: Soft_Reset rejected; unplug/replug C1 to recover\n");
            session->mode = MODE_SOFT_RESET_STALLED;
            session->pdo_index = 0xffu;
        } else if (rc == eUSBPD_OK) {
            if (session->soft_reset_attempted && USBPD_DirectPPSContractReady()) {
                /* Direct PPS achieved PS_RDY: do not issue another PPS
                 * Request here; subsequent refreshes remain unchanged. */
                session->pdo_index = (uint8_t)choose_pps_pdo(
                    session->desired_mv, session->desired_ma, true);
                if (session->pdo_index == 0xffu) {
                    printf("ERROR: direct PPS contract lost its APDO; unplug C1\n");
                    session->mode = MODE_SOFT_RESET_STALLED;
                    return;
                }
                session->mode = MODE_READY;
                session->last_pps_ms = s_ms;
                funDigitalWrite(LED_PIN, LED_ON);
                printf("PD: direct PPS after Soft_Reset confirmed 7000mV/1000mA\n");
                print_status(session);
                printf("VI> ");
            } else {
                accept_pps_after_fixed_contract(session);
            }
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
            if (session->soft_reset_attempted) {
                printf("PD: Soft_Reset recovery incomplete; unplug/replug C1\n");
                session->mode = MODE_SOFT_RESET_STALLED;
                session->pdo_index = 0xffu;
            } else {
                schedule_retry(session, "negotiation timeout");
            }
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

/* M4: a read-only snapshot of Source_Capabilities for the PDO list UI. */
static void m4_sync_source(const Session *session)
{
    const bool pd_ready = session->mode == MODE_READY &&
                          USBPD_GetState() == eSTATE_PS_RDY;
    m4_ui_set_pd(&s_m4_ui, session->desired_mv, session->desired_ma, pd_ready);
    if (session->mode != MODE_READY && session->mode != MODE_UNSUPPORTED) {
        m4_ui_set_pdos(&s_m4_ui, NULL, 0u);
        s_m4_caps_sampled = false;
        return;
    }
    /* Source list is static between PD events; don't scan on every 2ms tick. */
    if (s_m4_caps_sampled &&
        (uint32_t)(s_ms - s_m4_last_caps_ms) < 250u) return;
    s_m4_last_caps_ms = s_ms;
    s_m4_caps_sampled = true;
    USBPD_SPR_CapabilitiesMessage_t caps_snapshot;
    const size_t count = USBPD_CopyCapabilities(&caps_snapshot);
    USBPD_SPR_CapabilitiesMessage_t *caps = count ? &caps_snapshot : NULL;
    m4_pdo_t copy[M4_MAX_PDOS];
    uint8_t stored = 0u;
    if (caps) {
        for (size_t i = 0u; i < count && stored < M4_MAX_PDOS; ++i) {
            const USBPD_SourcePDO_t *pdo = &caps->Source[i];
            m4_pdo_t item = {0};
            item.source_index = (uint8_t)i;
            if (USBPD_IsPPS(pdo)) {
                item.kind = M4_PDO_PPS;
                item.min_mv = pdo->SPR_PPS.MinVoltageIn100mV * 100u;
                item.max_mv = pdo->SPR_PPS.MaxVoltageIn100mV * 100u;
                item.max_ma = pdo->SPR_PPS.MaxCurrentIn50mA * 50u;
            } else if (pdo->Header.PDOType == eUSBPD_PDO_FIXED) {
                item.kind = M4_PDO_FIXED;
                item.min_mv = pdo->FixedSupply.VoltageIn50mV * 50u;
                item.max_mv = item.min_mv;
                item.max_ma = pdo->FixedSupply.MaxCurrentIn10mA * 10u;
            } else {
                item.kind = M4_PDO_OTHER;
            }
            copy[stored++] = item;
        }
    }
    m4_ui_set_pdos(&s_m4_ui, copy, stored);
}

/* Print from the main loop, never from the USBPD interrupt handler.
 * The latest completed Source_Capabilities is retained, so the reported
 * count can be compared with 2+4*N and 2+4*N+4 on a real charger. */
static void m4c_rx_diag_service(void)
{
#if M4C_RX_DIAGNOSTIC
    static uint32_t read_seq;
    uint32_t written = s_m4c_rx_trace_written;
    if ((uint32_t)(written - read_seq) > M4C_RX_TRACE_SLOTS) {
        const uint32_t skipped = written - read_seq - M4C_RX_TRACE_SLOTS;
        printf("M4C TRACE: lost=%lu events (ring overflow)\n", (unsigned long)skipped);
        read_seq = written - M4C_RX_TRACE_SLOTS;
    }
    /* Keep debugprintf from monopolizing the foreground task.  USB PD
     * protocol responses have a deadline; one slow terminal dump must not
     * delay the next tick_session() call.  Pending records remain in the ring
     * (subject to its existing overflow accounting). */
    uint8_t emitted = 0u;
    while (read_seq != written && emitted < 2u) {
        /* A Source_Capabilities frame can arrive asynchronously while we
         * drain an older trace.  Give its Request priority on the next tick. */
        if (USBPD_GetState() == eSTATE_SOURCE_CAP)
            break;
        ++emitted;
        const uint32_t seq = read_seq;
        const volatile M4C_RxTrace_t *src =
            &s_m4c_rx_trace[seq % M4C_RX_TRACE_SLOTS];
        const uint16_t bytes = src->rx_bytes;
        const uint8_t n = src->pdo_count;
        const uint8_t verdict = src->verdict;
        const uint8_t msg_id = src->msg_id;
        const uint8_t prior_state = src->prior_state;
        const uint32_t last_pdo = src->last_pdo;
        const uint16_t raw_header = src->raw_header;
        const uint16_t irq_status = src->irq_status;
        const uint16_t irq_control = src->irq_control;
        const uint16_t irq_config = src->irq_config;
        uint8_t raw_pdo_count = src->raw_pdo_count;
        uint32_t raw_pdos[PD_CAPS_MAX_PDOS];
        if (raw_pdo_count > PD_CAPS_MAX_PDOS) raw_pdo_count = 0u;
        if (verdict == 1u || verdict == 2u || verdict == 3u || verdict == 9u || verdict == 10u) {
            for (uint8_t j = 0u; j < raw_pdo_count; ++j)
                raw_pdos[j] = src->raw_pdos[j];
        }
        __asm volatile( "" ::: "memory" );
        if (s_m4c_rx_trace_written - seq > M4C_RX_TRACE_SLOTS) {
            printf("M4C TRACE: overwritten event=%lu\n", (unsigned long)(seq + 1u));
            read_seq = s_m4c_rx_trace_written - M4C_RX_TRACE_SLOTS;
            written = s_m4c_rx_trace_written;
            continue;
        }
        ++read_seq;
        if (verdict == 19u) {
            printf("M4C LOOP: at_RX_RESET stage=%u stage_age_ticks=%lu "
                   "clock_hz=%lu pd_state=%u\n",
                   (unsigned)bytes, (unsigned long)last_pdo,
                   (unsigned long)FUNCONF_SYSTEM_CORE_CLOCK,
                   (unsigned)prior_state);
            continue;
        }
        if (verdict == 17u) {
            printf("M4C PATH: main_calls=%u negotiate_calls=%u "
                   "sourcecap_branch=%u last_mode=%u pd_state=%u "
                   "first_main_ticks=%lu clock_hz=%lu\n",
                   (unsigned)(bytes & 0xffu), (unsigned)(bytes >> 8),
                   (unsigned)n, (unsigned)msg_id, (unsigned)prior_state,
                   (unsigned long)last_pdo,
                   (unsigned long)FUNCONF_SYSTEM_CORE_CLOCK);
            continue;
        }
        if (verdict == 18u) {
            printf("M4C PATH: first_negotiate_ticks=%lu valid=%u "
                   "pd_state=%u clock_hz=%lu\n",
                   (unsigned long)last_pdo, (unsigned)n,
                   (unsigned)prior_state,
                   (unsigned long)FUNCONF_SYSTEM_CORE_CLOCK);
            continue;
        }
        if (verdict == 14u || verdict == 15u || verdict == 16u) {
            const char *event = verdict == 14u ? "CAPS_GOODCRC_TX_END" :
                                (verdict == 15u ? "FIRST_REQUEST_START" :
                                 "RX_RESET_CAPS_DEADLINE");
            printf("M4C DEADLINE: seq=%lu event=%s age_ticks=%lu "
                   "clock_hz=%lu request_started=%u state=%u\n",
                   (unsigned long)(seq + 1u), event,
                   (unsigned long)last_pdo,
                   (unsigned long)FUNCONF_SYSTEM_CORE_CLOCK,
                   (unsigned)(verdict == 16u ? n : (verdict == 15u)),
                   (unsigned)prior_state);
            continue;
        }
        if (verdict == 13u) {
            const uint32_t since_tx = (uint32_t)irq_status |
                                      ((uint32_t)irq_control << 16);
            /* Clock ticks, not milliseconds; 0xffffffff means unavailable.
             * FUNCONF_SYSTEM_CORE_CLOCK converts ticks to seconds. */
            printf("M4C TRACE: seq=%lu IRQ=RX_RESET_INTERVAL "
                   "since_prev_ticks=%lu since_txend_ticks=%lu "
                   "clock_hz=%lu last_tx_kind=%u "
                   "irq_status=0x%04X control_low=0x%02X "
                   "config_low=0x%02X state=%u\n",
                   (unsigned long)(seq + 1u), (unsigned long)last_pdo,
                   (unsigned long)since_tx,
                   (unsigned long)FUNCONF_SYSTEM_CORE_CLOCK,
                   (unsigned)raw_header, (unsigned)bytes,
                   (unsigned)n, (unsigned)msg_id,
                   (unsigned)prior_state);
            continue;
        }
        if (verdict == 12u) {
            printf("M4C TRACE: seq=%lu IRQ=RX_RESET_POSTCLEAR "
                   "status_before=0x%04X status_after=0x%04X "
                   "reset_flag_after=%u bmc_count=%u state=%u "
                   "control_low=0x%02X config_low=0x%02X\n",
                   (unsigned long)(seq + 1u),
                   (unsigned)(last_pdo & 0xffffu), (unsigned)bytes,
                   (unsigned)((bytes & IF_RX_RESET) != 0u),
                   (unsigned)((last_pdo >> 16) & 0xffffu),
                   (unsigned)prior_state, (unsigned)n, (unsigned)msg_id);
            continue;
        }
        if (verdict == 11u) {
            const bool probable_hard_reset = (bytes & 3u) == 2u;
            printf("M4C TRACE: seq=%lu IRQ=RX_RESET status=0x%04X "
                   "bmc_aux=%u interpretation=%s "
                   "control_low=0x%02X config_low=0x%02X state=%u "
                   "irq_entry=0x%04X bmc_count=%u\n",
                   (unsigned long)(seq + 1u), (unsigned)bytes,
                   (unsigned)(bytes & 3u),
                   probable_hard_reset ? "HARD_RESET_LATCHED" : "OTHER_RESET",
                   (unsigned)n, (unsigned)msg_id, (unsigned)prior_state,
                   (unsigned)(last_pdo & 0xffffu),
                   (unsigned)((last_pdo >> 16) & 0xffffu));
            continue;
        }
        if (verdict == 7u) {
            const char *kind = n == 1u ? "GOODCRC" :
                               n == 2u ? "REQUEST" :
                               n == 3u ? "GET_SOURCE_CAP" :
                               n == 5u ? "SOFT_RESET" : "OTHER";
            printf("M4C TRACE: seq=%lu IRQ=TX_END tx=%s msg_id=%u "
                   "state=%u tx_en=%u size=%lu rx_bytes=%u\n",
                   (unsigned long)(seq + 1u), kind, (unsigned)msg_id,
                   (unsigned)prior_state,
                   (unsigned)((last_pdo >> 8) & 1u),
                   (unsigned long)((last_pdo >> 16) & 255u),
                   (unsigned)bytes);
            continue;
        }
        if (verdict == 8u) {
            printf("M4C TRACE: seq=%lu TX=GOODCRC msg_id=%u state=%u size=%u\n",
                   (unsigned long)(seq + 1u), (unsigned)msg_id,
                   (unsigned)prior_state, (unsigned)bytes);
            continue;
        }
        if (verdict == 5u) {
            const char *type = n ? "REQUEST" :
                (last_pdo == eUSBPD_CTRL_MSG_GET_SOURCE_CAP ?
                 "GET_SOURCE_CAP" :
                 last_pdo == eUSBPD_CTRL_MSG_SOFT_RESET ?
                 "SOFT_RESET" : "OTHER");
            printf("M4C TRACE: seq=%lu TX=%s msg_id=%u state_before=%u "
                   "size=%u rdo=0x%08lX\n",
                   (unsigned long)(seq + 1u), type, (unsigned)msg_id,
                   (unsigned)prior_state, (unsigned)bytes,
                   (unsigned long)(n ? last_pdo : 0u));
            continue;
        }
        if (verdict == 6u) {
            printf("M4C TRACE: seq=%lu RX=GOODCRC msg_id=%u "
                   "tx_id_before=%lu tx_id_after=%lu match=%u state_before=%u bytes=%u\n",
                   (unsigned long)(seq + 1u), (unsigned)msg_id,
                   (unsigned long)(last_pdo & 7u),
                   (unsigned long)(((last_pdo & 7u) + ((last_pdo >> 8) & 1u)) & 7u),
                   (unsigned)((last_pdo >> 8) & 1u),
                   (unsigned)prior_state, (unsigned)bytes);
            continue;
        }
        if (verdict == 4u) {
            const char *name = (last_pdo == eUSBPD_CTRL_MSG_ACCEPT) ? "ACCEPT" :
                               (last_pdo == eUSBPD_CTRL_MSG_PS_RDY) ? "PS_RDY" :
                               (last_pdo == eUSBPD_CTRL_MSG_REJECT) ? "REJECT" :
                               (last_pdo == eUSBPD_CTRL_MSG_WAIT) ? "WAIT" : "OTHER";
            printf("M4C TRACE: seq=%lu CTRL=%s msg_id=%u state_before=%u "
                   "BMC_BYTE_CNT=%u\n", (unsigned long)(seq + 1u), name,
                   (unsigned)msg_id, (unsigned)prior_state, (unsigned)bytes);
            continue;
        }
        printf("M4C TRACE: seq=%lu msg_id=%u state_before=%u PDOs=%u "
               "BMC_BYTE_CNT=%u lastPDO=0x%08lX result=%s\n",
               (unsigned long)(seq + 1u), (unsigned)msg_id,
               (unsigned)prior_state, (unsigned)n, (unsigned)bytes,
               (unsigned long)last_pdo,
               verdict == 3u ? "accepted" :
               verdict == 9u ? "duplicate_ignored" :
               verdict == 10u ? "unchanged_ready_ignored" :
               verdict == 2u ? "rejected_PDO" : "rejected_length");
        if (verdict == 1u || verdict == 2u || verdict == 3u || verdict == 9u || verdict == 10u) {
            printf("M4C CAPS RAW: seq=%lu hdr=0x%04X status=0x%04X "
                   "control=0x%04X config=0x%04X captured=%u\n",
                   (unsigned long)(seq + 1u), (unsigned)raw_header,
                   (unsigned)irq_status, (unsigned)irq_control,
                   (unsigned)irq_config, (unsigned)raw_pdo_count);
            for (uint8_t j = 0u; j < raw_pdo_count; ++j)
                printf("M4C CAPS RAW: seq=%lu PDO[%u]=0x%08lX\n",
                       (unsigned long)(seq + 1u), (unsigned)j,
                       (unsigned long)raw_pdos[j]);
        }
    }
    static uint32_t last_seen;
    const uint32_t seq = s_m4c_diag_seq;
    if (seq == last_seen) return;
    const uint16_t bytes = s_m4c_diag_bytes;
    const uint8_t n = s_m4c_diag_pdos;
    const uint8_t verdict = s_m4c_diag_verdict;
    __asm volatile( "" ::: "memory" );
    if (s_m4c_diag_seq != seq) return;
    last_seen = seq;
    printf("M4C RX: PDOs=%u BMC_BYTE_CNT=%u noCRC=%u withCRC=%u result=%s\n",
           (unsigned)n, (unsigned)bytes,
           (unsigned)(2u + 4u * n), (unsigned)(6u + 4u * n),
           verdict == 3u ? "accepted" :
           verdict == 2u ? "rejected_PDO" : "rejected_length");
#endif
}

static void m4_process_pending(Session *session)
{
    const m4_action_t action = s_m4_pending;
    /* One-shot: a held button / subsequent redraw cannot resend. */
    s_m4_pending.apply = false;
    if (!action.apply) return;
#if M4_PPS_APPLY_ENABLE
    const bool ready = (session->mode == MODE_READY) &&
                       (USBPD_GetState() == eSTATE_PS_RDY) &&
                       USBPD_ActiveCCPresent();
    const bool target_unchanged = session->desired_mv == action.original_mv &&
                                  session->desired_ma == action.original_ma;

    /* Look up the exact ORIGINAL source PDO index and recheck its content.
     * Never choose a different APDO silently when the charger changes its
     * Source Capabilities in the middle of a UI edit. */
    USBPD_SPR_CapabilitiesMessage_t caps_snapshot;
    const size_t count = USBPD_CopyCapabilities(&caps_snapshot);
    USBPD_SPR_CapabilitiesMessage_t *caps = count ? &caps_snapshot : NULL;
    bool same_apdo = false;
    bool power_limited = false;
    PPSApdoLimits limits = {0};
    if (caps && (size_t)action.source_index < count) {
        const USBPD_SourcePDO_t *const pdo = &caps->Source[action.source_index];
        if (USBPD_IsPPS(pdo)) {
            limits.min_mv = pdo->SPR_PPS.MinVoltageIn100mV * 100u;
            limits.max_mv = pdo->SPR_PPS.MaxVoltageIn100mV * 100u;
            limits.max_ma = pdo->SPR_PPS.MaxCurrentIn50mA * 50u;
            power_limited = pdo->SPR_PPS.PPSpowerLimited;
            same_apdo = limits.min_mv == action.apdo_min_mv &&
                        limits.max_mv == action.apdo_max_mv &&
                        limits.max_ma == action.apdo_max_ma;
        }
    }
    const m4_apply_check_t check = {
        .pd_ready = ready,
        .original_target_unchanged = target_unchanged,
        .source_apdo_unchanged = same_apdo,
        .source_power_limited = power_limited,
        .apdo = limits,
        .mv = action.mv,
        .ma = action.ma,
    };
    const m4_apply_verdict_t verdict = m4_apply_validate(&check);
    if (verdict != M4_APPLY_OK) {
        m4_ui_result(&s_m4_ui, m4_apply_message(verdict));
        printf("M4B: rejected (%s), %lumV / %lumA; no request sent\n",
               m4_apply_message(verdict), (unsigned long)action.mv,
               (unsigned long)action.ma);
        return;
    }
    if (session->desired_mv == action.mv && session->desired_ma == action.ma &&
        session->pdo_index == action.source_index) {
        m4_ui_result(&s_m4_ui, "PPS UNCHANGED");
        printf("M4B: no-op, request unchanged\n");
        return;
    }
    printf("M4B: apply confirmed by PUSH: PDO[%u] %lumV / %lumA (PD request)\n",
           (unsigned)action.source_index,
           (unsigned long)action.mv, (unsigned long)action.ma);
    funDigitalWrite(LED_PIN, LED_OFF);
    if (!send_and_wait_pps(action.source_index, action.mv, action.ma, "UI set")) {
        /* IMPORTANT: retain the LAST CONFIRMED target, not a failed draft.
         * Hardware state may be uncertain after PD timeout; force renegotiate.
         * Original Session reconnect behavior restores the prior target. */
        m4_ui_result(&s_m4_ui, "PPS FAILED / RETRY");
        schedule_retry(session, "UI PPS change failed");
        return;
    }
    /* Only publish & persist a new desired target after PS_RDY confirmation.
     * BUS voltage is checked separately with INA226 in the field trial. */
    session->desired_mv = action.mv;
    session->desired_ma = action.ma;
    session->pdo_index = action.source_index;
    session->mode = MODE_READY;
    session->last_pps_ms = s_ms;
    funDigitalWrite(LED_PIN, LED_ON);
    m4_ui_result(&s_m4_ui, "PPS APPLIED");
    printf("M4B: PPS set %lumV / %lumA, PS_RDY confirmed (BUS unverified)\n",
           (unsigned long)action.mv, (unsigned long)action.ma);
#else
    (void)session;
    printf("M4B: preview-only mode, no request sent\n");
    m4_ui_result(&s_m4_ui, "DEMO - NO PPS CHANGE");
#endif
}

int main(void)
{
    SystemInit();
    led_init();
    systick_init_ms();
    adc_voltage_init_pa0();
    ina226_bus_init();
    m3_gpio_init();
    m3_input_init(&s_m3_input, m3_gpio_read(), s_ms);
    m4_ui_init(&s_m4_ui, INITIAL_VOLTAGE_MV, INITIAL_CURRENT_MA);
    s_m3_last_poll_ms = s_ms;
    WaitForDebuggerToAttach(0);
    printf("BOOT: CH32X035 M4C PPS TRIAL (7000mV/1000mA ONLY)\n");
#if M4_PPS_APPLY_ENABLE
    printf("M4B: UI live apply enabled; no load during first field tests\n");
#else
    printf("M4B: UI preview only; live apply disabled\n");
#endif
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
        m4c_loop_checkpoint(1u);
        poll_input();
        m4c_loop_checkpoint(2u);
        process_command(&session);
        m4c_loop_checkpoint(3u);
        tick_session(&session);
        m4c_loop_checkpoint(4u);
        /* Manual Soft_Reset leaves the session in MODE_NEGOTIATING.  Never
         * drain the debugprintf ring while waiting for the next PD response:
         * in field tests it occupied stage 4 for ~29 ms, leaving no chance
         * for tick_session() to send Request after Source_Capabilities.
         * Keep collecting trace records in the ISR; flush after the contract
         * is ready or has entered its existing fail-closed state. */
        if (session.mode != MODE_NEGOTIATING &&
            USBPD_GetState() != eSTATE_SOURCE_CAP)
            m4c_rx_diag_service();
        m4c_loop_checkpoint(5u);
        m4c_vbus_diag_service(&session);
        m4c_loop_checkpoint(6u);
        m4_sync_source(&session);
        m4c_loop_checkpoint(7u);
        m3_service();
        m4c_loop_checkpoint(8u);
        m4_process_pending(&session);
#if M2_OLED_ENABLE
        m4c_loop_checkpoint(9u);
        m2_service(&session);
#endif
        /* Keep CC detection/initial PD negotiation responsive. */
        m4c_loop_checkpoint(10u);
        Delay_Ms(session.mode == MODE_NEGOTIATING ? 1u : 2u);
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
