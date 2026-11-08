/*
 * 06_pps_current_limit - WeAct CH32X035F8U6 CoreBoard
 * Request PPS 7.00 V and alternate operating-current limit between
 * 1.80 A and 1.00 A every 10 seconds, refreshing the PPS contract
 * every 3 seconds between changes.
 *
 * Based on user's known-working 04_pps_6v and 05_pps_sweep,
 * using a local copy of Bogdan Ionescu's usbpd.h with added
 * USBPD_SelectPPS(index, voltage_mV, current_mA).
 *
 * The RDO sets a limit, NOT forced load current. USBPD_SelectPPS()
 * succeeds when a Request is submitted; wait for PS_RDY separately.
 *
 * Status LED: PB12 (IC physical pin 15). HIGH=ON on this board.
 */

#include "ch32fun.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define USBPD_IMPLEMENTATION
#include "usbpd_current.h"

#define LED_PIN                 PB12
#define LED_ON                  FUN_HIGH
#define LED_OFF                 FUN_LOW

#define PPS_VOLTAGE_MV          7000u
#define PPS_CURRENT_HIGH_MA    1800u
#define PPS_CURRENT_LOW_MA     1000u
#define PPS_REFRESH_PERIOD_MS  3000u
#define PPS_SWITCH_PERIOD_MS  10000u
#define PPS_RESPONSE_TIMEOUT_MS 2000u
#define PPS_SEND_TIMEOUT_MS    1000u
#define NEGOTIATE_TIMEOUT_MS   20000u

static volatile uint32_t s_ms = 0u;
static bool debugger_attached = false;

#define LOG(fmt, ...) do { \
    if (debugger_attached) printf(fmt "\n", ##__VA_ARGS__); \
} while (0)

static void systick_init_ms(void);

static void led_init(void)
{
    funGpioInitAll();
    funPinMode(LED_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funDigitalWrite(LED_PIN, LED_OFF);
}

static void led_error_forever(unsigned blinks)
{
    while (1)
    {
        for (unsigned i = 0u; i < blinks; ++i)
        {
            funDigitalWrite(LED_PIN, LED_ON);
            Delay_Ms(250);
            funDigitalWrite(LED_PIN, LED_OFF);
            Delay_Ms(250);
        }
        Delay_Ms(2000);
    }
}

/* 1.8 A requested: steady ON; 1.0 A requested: slow blink (not measured!). */
static void led_update(uint32_t current_ma, uint32_t now_ms)
{
    if (current_ma == PPS_CURRENT_HIGH_MA)
        funDigitalWrite(LED_PIN, LED_ON);
    else
        funDigitalWrite(LED_PIN, (now_ms % 1000u) < 500u ? LED_ON : LED_OFF);
}

static int find_pps_pdo(void)
{
    USBPD_SPR_CapabilitiesMessage_t *caps = NULL;
    const size_t count = USBPD_GetCapabilities(&caps);
    if (!caps || count == 0u) return -1;

    LOG("PD: Source PDO count=%u", (unsigned)count);
    for (size_t i = 0u; i < count; ++i)
    {
        const USBPD_SourcePDO_t *pdo = &caps->Source[i];
        if (!USBPD_IsPPS(pdo)) continue;

        const uint32_t min_mv = pdo->SPR_PPS.MinVoltageIn100mV * 100u;
        const uint32_t max_mv = pdo->SPR_PPS.MaxVoltageIn100mV * 100u;
        const uint32_t max_ma = pdo->SPR_PPS.MaxCurrentIn50mA * 50u;
        LOG("PD: PDO[%u] PPS %lu-%lu mV, max %lu mA",
            (unsigned)i, (unsigned long)min_mv,
            (unsigned long)max_mv, (unsigned long)max_ma);

        if (min_mv <= PPS_VOLTAGE_MV && max_mv >= PPS_VOLTAGE_MV &&
            max_ma >= PPS_CURRENT_HIGH_MA)
        {
            return (int)i;
        }
    }
    return -1;
}

/* Return true only after we observe the Source's PS_RDY response.
 * PS_RDY implies the protocol exchange completed, NOT that VBUS was
 * independently measured. Errors include Reject/Wait and timeout.
 */
static bool send_and_wait_pps(uint8_t index, uint32_t current_ma, const char *event)
{
    USBPD_Result_e result = eUSBPD_BUSY;
    const uint32_t send_start_ms = s_ms;
    do
    {
        result = USBPD_SelectPPS(index, PPS_VOLTAGE_MV, current_ma);
        if (result != eUSBPD_BUSY) break;
        Delay_Ms(5);
    } while ((uint32_t)(s_ms - send_start_ms) < PPS_SEND_TIMEOUT_MS);

    LOG("PPS: %s request 7000mV / %lumA, send=%s, state=%s",
        event, (unsigned long)current_ma,
        USBPD_ResultToStr(result), USBPD_StateToStr(USBPD_GetState()));

    if (result != eUSBPD_OK) return false;

    const uint32_t response_start_ms = s_ms;
    while ((uint32_t)(s_ms - response_start_ms) < PPS_RESPONSE_TIMEOUT_MS)
    {
        const USBPD_State_e state = USBPD_GetState();
        if (state == eSTATE_PS_RDY)
        {
            LOG("PPS: %s PS_RDY 7000mV / %lumA", event,
                (unsigned long)current_ma);
            return true;
        }
        if (state != eSTATE_WAIT_ACCEPT && state != eSTATE_WAIT_PS_RDY)
        {
            LOG("ERROR: %s interrupted: state=%s", event,
                USBPD_StateToStr(state));
            return false;
        }
        Delay_Ms(10);
    }

    LOG("ERROR: %s PPS response timed out (state=%s)", event,
        USBPD_StateToStr(USBPD_GetState()));
    return false;
}

int main(void)
{
    SystemInit();
    led_init();
    systick_init_ms();
    debugger_attached = !WaitForDebuggerToAttach(300);

    LOG("BOOT: CH32X035 PPS current-limit experiment");
    LOG("Config: 7000mV, 1800mA <-> 1000mA every 10s, refresh every 3s");

    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA;
    USBPD_Result_e result = USBPD_Init(eUSBPD_VCC_3V3);
    LOG("PD: init=%s", USBPD_ResultToStr(result));
    if (result != eUSBPD_OK) led_error_forever(5);

    USBPD_Reset();
    const uint32_t negotiate_start_ms = s_ms;
    do
    {
        result = USBPD_SinkNegotiate();
        if ((uint32_t)(s_ms - negotiate_start_ms) >= NEGOTIATE_TIMEOUT_MS)
        {
            LOG("ERROR: initial PD negotiation timeout");
            led_error_forever(5);
        }
    } while (result == eUSBPD_BUSY);
    LOG("PD: negotiate=%s, state=%s", USBPD_ResultToStr(result),
        USBPD_StateToStr(USBPD_GetState()));
    if (result != eUSBPD_OK) led_error_forever(5);

    const int pdo_index = find_pps_pdo();
    if (pdo_index < 0)
    {
        LOG("ERROR: no PPS APDO supports 7.00V and 1.80A");
        led_error_forever(2);
    }

    uint32_t current_ma = PPS_CURRENT_HIGH_MA;
    if (!send_and_wait_pps((uint8_t)pdo_index, current_ma, "start"))
        led_error_forever(5);

    uint32_t last_request_ms = s_ms;
    uint32_t last_switch_ms = s_ms;
    while (1)
    {
        const uint32_t now_ms = s_ms;
        led_update(current_ma, now_ms);

        if ((uint32_t)(now_ms - last_switch_ms) >= PPS_SWITCH_PERIOD_MS)
        {
            const uint32_t next_ma = (current_ma == PPS_CURRENT_HIGH_MA)
                ? PPS_CURRENT_LOW_MA : PPS_CURRENT_HIGH_MA;
            if (!send_and_wait_pps((uint8_t)pdo_index, next_ma, "switch"))
                led_error_forever(5);
            current_ma = next_ma;
            last_switch_ms = s_ms;
            last_request_ms = s_ms;
        }
        else if ((uint32_t)(now_ms - last_request_ms) >= PPS_REFRESH_PERIOD_MS)
        {
            if (!send_and_wait_pps((uint8_t)pdo_index, current_ma, "refresh"))
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
    s_ms = 0;
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
