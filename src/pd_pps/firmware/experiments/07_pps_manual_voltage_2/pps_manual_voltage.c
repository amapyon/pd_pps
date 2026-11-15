/*
 * 07_pps_manual_voltage - CH32X035F8U6 CoreBoard
 * Enter a PPS voltage on the minichlink -T debug console.
 * Exactly 20mV per PPS RDO unit.  The APDO limit is read from source.
 * No extra UART wires: debug input uses WCH-LinkE/SWIO (pin number depends
 * on chosen board package). Status LED: PB12 (physical IC pin 15).
 *
 * Based on verified 06_pps_current_limit and its usbpd_current.h.
 * This firmware has not been tested on the user's actual hardware yet.
 */
#include "ch32fun.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "console_parse.h"

#define USBPD_IMPLEMENTATION
#include "usbpd_current.h"

#define LED_PIN                    PB12
#define LED_ON                     FUN_HIGH
#define LED_OFF                    FUN_LOW
#define INITIAL_VOLTAGE_MV         7000u
#define PPS_REQUEST_CURRENT_MA     1000u  /* Contract limit, not actual load current. */
#define PPS_REFRESH_PERIOD_MS      3000u
#define PPS_RESPONSE_TIMEOUT_MS    2000u
#define PPS_SEND_TIMEOUT_MS        1000u
#define NEGOTIATE_TIMEOUT_MS      20000u
#define LINE_BUFFER_SIZE             32u

static volatile uint32_t s_ms;
static char s_line[LINE_BUFFER_SIZE];
static unsigned s_line_length;
static bool s_line_ready;
static bool s_line_overflow;
static bool s_last_was_newline;

/* Weak callback in ch32fun.c: called during poll_input() or printf().
 * Do not print from here: a printf could re-enter the input handler.
 */
void handle_debug_input(int length, uint8_t *bytes)
{
    for (int i = 0; i < length; ++i) {
        const uint8_t c = bytes[i];
        if (s_line_ready) continue;
        if (c == '\r' || c == '\n') {
            if (s_last_was_newline) continue; /* handles CRLF */
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
        if (s_line_length + 1u < LINE_BUFFER_SIZE) {
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

static void led_error_forever(unsigned count)
{
    printf("FATAL: halting firmware. Disconnect charger before recovery.\n");
    for (;;) {
        for (unsigned i = 0; i < count; ++i) {
            funDigitalWrite(LED_PIN, LED_ON);
            Delay_Ms(200);
            funDigitalWrite(LED_PIN, LED_OFF);
            Delay_Ms(200);
        }
        Delay_Ms(1500);
    }
}

/* Choose an APDO supporting requested voltage/current. Prefer higher max
 * advertised current where multiple 45W PPS APDOs overlap.
 */
static int choose_pps_pdo(uint32_t mv, bool dump)
{
    USBPD_SPR_CapabilitiesMessage_t *caps = NULL;
    const size_t count = USBPD_GetCapabilities(&caps);
    if (!caps || !count) return -1;
    if (dump) printf("PD: Source PDO count=%u\n", (unsigned)count);

    int best = -1;
    uint32_t best_ma = 0u;
    for (size_t i = 0; i < count; ++i) {
        const USBPD_SourcePDO_t *pdo = &caps->Source[i];
        if (!USBPD_IsPPS(pdo)) continue;
        const uint32_t min_mv = pdo->SPR_PPS.MinVoltageIn100mV * 100u;
        const uint32_t max_mv = pdo->SPR_PPS.MaxVoltageIn100mV * 100u;
        const uint32_t max_ma = pdo->SPR_PPS.MaxCurrentIn50mA * 50u;
        if (dump) printf("PD: PDO[%u] PPS %lu-%lu mV, max %lu mA\n",
            (unsigned)i, (unsigned long)min_mv, (unsigned long)max_mv,
            (unsigned long)max_ma);
        if (mv >= min_mv && mv <= max_mv &&
            max_ma >= PPS_REQUEST_CURRENT_MA && max_ma > best_ma) {
            best = (int)i;
            best_ma = max_ma;
        }
    }
    return best;
}

/* PS_RDY confirms USB-PD negotiation; it is not a VBUS measurement. */
static bool send_and_wait_pps(uint8_t index, uint32_t mv, const char *reason)
{
    USBPD_Result_e result = eUSBPD_BUSY;
    uint32_t start = s_ms;
    do {
        result = USBPD_SelectPPS(index, mv, PPS_REQUEST_CURRENT_MA);
        if (result != eUSBPD_BUSY) break;
        Delay_Ms(5);
    } while ((uint32_t)(s_ms - start) < PPS_SEND_TIMEOUT_MS);

    const bool quiet_refresh = (strcmp(reason, "refresh") == 0);
    if (!quiet_refresh || result != eUSBPD_OK)
        printf("PPS: %s %lu mV / %u mA, send=%s, state=%s\n",
            reason, (unsigned long)mv, PPS_REQUEST_CURRENT_MA,
            USBPD_ResultToStr(result), USBPD_StateToStr(USBPD_GetState()));
    if (result != eUSBPD_OK) return false;

    start = s_ms;
    while ((uint32_t)(s_ms - start) < PPS_RESPONSE_TIMEOUT_MS) {
        USBPD_State_e st = USBPD_GetState();
        if (st == eSTATE_PS_RDY) {
            if (!quiet_refresh)
                printf("PPS: %s PS_RDY at requested %lu mV\n",
                    reason, (unsigned long)mv);
            return true;
        }
        if (st != eSTATE_WAIT_ACCEPT && st != eSTATE_WAIT_PS_RDY) {
            printf("ERROR: PD state=%s\n", USBPD_StateToStr(st));
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
    printf("Type voltage then ENTER, e.g. 7.02 or 7020 (mV).\n");
    printf("  7.00  => 7000mV; 7.02 => 7020mV; 11.00 => 11000mV\n");
    printf("  status (or s) : report selected contract\n");
    printf("  help   (or ?) : show commands\n");
    printf("  Units: PPS step=20mV; voltage must be within source APDO.\n");
    printf("  WARNING: PS_RDY is not a voltage measurement.\n");
}

static void print_status(uint32_t mv, uint8_t index)
{
    printf("STATUS: requested=%lu mV, current limit=%u mA, APDO index=%u, PD=%s\n",
        (unsigned long)mv, PPS_REQUEST_CURRENT_MA, (unsigned)index,
        USBPD_StateToStr(USBPD_GetState()));
}

int main(void)
{
    SystemInit();
    led_init();
    systick_init_ms();

    /* An interactive device should start only when the console is attached.
     * Attach minichlink -T and leave it running for the experiment.
     */
    WaitForDebuggerToAttach(0);
    printf("BOOT: CH32X035 PPS manual voltage console\n");
    printf("PPS RDO voltage resolution: 20mV\n");

    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA;
    USBPD_Result_e result = USBPD_Init(eUSBPD_VCC_3V3);
    printf("PD: init=%s\n", USBPD_ResultToStr(result));
    if (result != eUSBPD_OK) led_error_forever(5);
    USBPD_Reset();
    uint32_t start = s_ms;
    do {
        result = USBPD_SinkNegotiate();
        if ((uint32_t)(s_ms - start) > NEGOTIATE_TIMEOUT_MS) {
            printf("ERROR: initial PD negotiation timeout\n");
            led_error_forever(5);
        }
    } while (result == eUSBPD_BUSY);
    printf("PD: negotiate=%s, state=%s\n",
        USBPD_ResultToStr(result), USBPD_StateToStr(USBPD_GetState()));
    if (result != eUSBPD_OK) led_error_forever(5);

    uint32_t current_mv = INITIAL_VOLTAGE_MV;
    int initial_index = choose_pps_pdo(current_mv, true);
    if (initial_index < 0) {
        printf("ERROR: no PPS APDO supports initial %lumV/%umA\n",
            (unsigned long)current_mv, PPS_REQUEST_CURRENT_MA);
        led_error_forever(2);
    }
    uint8_t pdo_index = (uint8_t)initial_index;
    if (!send_and_wait_pps(pdo_index, current_mv, "start"))
        led_error_forever(5);
    funDigitalWrite(LED_PIN, LED_ON);
    uint32_t last_request_ms = s_ms;
    print_help();
    print_status(current_mv, pdo_index);
    printf("V> ");

    for (;;) {
        poll_input();
        if (s_line_ready) {
            char local[LINE_BUFFER_SIZE];
            const bool overflow = s_line_overflow;
            memcpy(local, s_line, sizeof(local));
            s_line_length = 0u;
            s_line_overflow = false;
            s_line_ready = false;

            if (overflow) {
                printf("ERROR: command too long\n");
            } else if (!strcmp(local, "help") || !strcmp(local, "?")) {
                print_help();
            } else if (!strcmp(local, "status") || !strcmp(local, "s")) {
                print_status(current_mv, pdo_index);
            } else if (local[0]) {
                uint32_t new_mv = 0u;
                if (!parse_voltage_mv(local, &new_mv)) {
                    printf("ERROR: enter volts (7.02) or millivolts (7020)\n");
                } else if (new_mv % 20u) {
                    printf("ERROR: PPS voltage must be a multiple of 20mV (got %lumV)\n",
                        (unsigned long)new_mv);
                } else {
                    const int new_index = choose_pps_pdo(new_mv, false);
                    if (new_index < 0) {
                        printf("ERROR: %lumV out of available PPS APDO range\n",
                            (unsigned long)new_mv);
                    } else if (new_mv == current_mv && (uint8_t)new_index == pdo_index) {
                        printf("PPS: unchanged (%lumV)\n", (unsigned long)current_mv);
                    } else {
                        funDigitalWrite(LED_PIN, LED_OFF);
                        if (!send_and_wait_pps((uint8_t)new_index, new_mv, "set"))
                            led_error_forever(5);
                        current_mv = new_mv;
                        pdo_index = (uint8_t)new_index;
                        last_request_ms = s_ms;
                        funDigitalWrite(LED_PIN, LED_ON);
                        print_status(current_mv, pdo_index);
                    }
                }
            }
            printf("V> ");
        }
        if ((uint32_t)(s_ms - last_request_ms) >= PPS_REFRESH_PERIOD_MS) {
            if (!send_and_wait_pps(pdo_index, current_mv, "refresh"))
                led_error_forever(5);
            last_request_ms = s_ms;
        }
        Delay_Ms(10);
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
