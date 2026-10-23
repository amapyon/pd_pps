/*
 * 05_pps_sweep - WeAct CH32X035F8U6 CoreBoard
 * USB PD/PPS Sink: sweep 5.00 V <-> 11.00 V in 0.50 V steps,
 * keeping each setting for approximately 3 seconds.
 *
 * Adapted from the 04_pps_6v application previously tested on this board.
 * The USB PD engine is provided by ch32fun's usbpd.h.
 *
 * PB12 (IC physical pin 15): status LED.
 * LED flashes once when a PPS request is issued. It does not measure VBUS.
 * Test WITHOUT an external load and verify VBUS with a USB PD meter.
 */

#include "ch32fun.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define USBPD_IMPLEMENTATION
#include "../ch32fun/examples_x035/usbpd_sink/usbpd.h"

#define LED_PIN                 PB12
#define LED_ON                  FUN_HIGH
#define LED_OFF                 FUN_LOW

#define PPS_MIN_VOLTAGE_MV      5000U
#define PPS_MAX_VOLTAGE_MV     11000U
#define PPS_STEP_MV             500U
#define PPS_STEP_PERIOD_MS     3000U
#define PPS_MIN_CURRENT_MA      500U  /* Checks supply capability, NOT the requested RDO current. */
#define NEGOTIATE_TIMEOUT_MS  20000U
#define LED_PULSE_MS            150U

#if ((PPS_MIN_VOLTAGE_MV % 100U) != 0U) || \
    ((PPS_MAX_VOLTAGE_MV % 100U) != 0U) || \
    ((PPS_STEP_MV % 100U) != 0U)
#error "ch32fun USBPD_SelectPDO() PPS voltage argument uses 100mV units"
#endif

static volatile uint32_t s_ms = 0;
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

/* 2 flashes: no suitable PPS PDO; 5 flashes: PD init/negotiation error. */
static void led_error_forever(unsigned blinks)
{
    while (1)
    {
        for (unsigned i = 0; i < blinks; ++i)
        {
            funDigitalWrite(LED_PIN, LED_ON);
            Delay_Ms(300);
            funDigitalWrite(LED_PIN, LED_OFF);
            if (i + 1U < blinks) Delay_Ms(300);
        }
        Delay_Ms(2500);
    }
}

/* Extract a 32-bit PDO from the same library representation as 04_pps_6v. */
static uint32_t pdo_raw(const USBPD_SourcePDO_t *pdo)
{
    const uint8_t *p = (const uint8_t *)pdo;
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* Find an SPR PPS APDO covering the entire 5.00 V to 11.00 V sweep. */
static int find_sweep_pps_pdo(void)
{
    USBPD_SPR_CapabilitiesMessage_t *caps = NULL;
    const size_t count = USBPD_GetCapabilities(&caps);
    if (!caps || count == 0U) return -1;

    LOG("PD: source PDO count=%u", (unsigned)count);
    for (size_t i = 0; i < count; ++i)
    {
        const USBPD_SourcePDO_t *pdo = &caps->Source[i];
        const uint32_t raw = pdo_raw(pdo);

        if (!USBPD_IsPPS(pdo))
        {
            LOG("PD: PDO[%u] not PPS, raw=0x%08lx", (unsigned)i,
                (unsigned long)raw);
            continue;
        }

        /* SPR PPS APDO: max/min voltage in 100mV, max current in 50mA. */
        const uint32_t min_mv = ((raw >> 8) & 0xFFU) * 100U;
        const uint32_t max_mv = ((raw >> 17) & 0xFFU) * 100U;
        const uint32_t max_ma = (raw & 0x7FU) * 50U;

        LOG("PD: PDO[%u] PPS %lu-%lu mV, max %lu mA, raw=0x%08lx",
            (unsigned)i, (unsigned long)min_mv, (unsigned long)max_mv,
            (unsigned long)max_ma, (unsigned long)raw);

        if (min_mv <= PPS_MIN_VOLTAGE_MV &&
            max_mv >= PPS_MAX_VOLTAGE_MV &&
            max_ma >= PPS_MIN_CURRENT_MA)
        {
            return (int)i;
        }
    }
    return -1;
}

/* Issue a PPS request. This library's PPS voltage argument is in 100mV units. */
static void request_voltage(int pdo_index, uint32_t voltage_mv)
{
    (void)USBPD_SelectPDO((size_t)pdo_index, voltage_mv / 100U);
    LOG("PPS: request %lu mV, state=%s",
        (unsigned long)voltage_mv, USBPD_StateToStr(USBPD_GetState()));
}

int main(void)
{
    SystemInit();
    led_init();
    systick_init_ms();

    debugger_attached = !WaitForDebuggerToAttach(300);
    LOG("BOOT: CH32X035 PPS sweep 5.00-11.00V, step 0.50V, period 3s");

    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA;

    USBPD_Result_e result = USBPD_Init(eUSBPD_VCC_3V3);
    LOG("PD: init=%s", USBPD_ResultToStr(result));
    if (result != eUSBPD_OK)
    {
        LOG("ERROR: PD init failed");
        led_error_forever(5);
    }

    USBPD_Reset();
    const uint32_t start_ms = s_ms;
    do
    {
        result = USBPD_SinkNegotiate();
        if ((uint32_t)(s_ms - start_ms) >= NEGOTIATE_TIMEOUT_MS)
        {
            LOG("ERROR: PD negotiation timed out");
            led_error_forever(5);
        }
    } while (result == eUSBPD_BUSY);

    LOG("PD: negotiate=%s state=%s", USBPD_ResultToStr(result),
        USBPD_StateToStr(USBPD_GetState()));
    if (result != eUSBPD_OK)
    {
        LOG("ERROR: PD negotiation failed");
        led_error_forever(5);
    }

    const int pdo_index = find_sweep_pps_pdo();
    if (pdo_index < 0)
    {
        LOG("ERROR: no PPS PDO covers 5.00-11.00V");
        led_error_forever(2);
    }

    LOG("PPS: selected APDO index=%d", pdo_index);

    uint32_t voltage_mv = PPS_MIN_VOLTAGE_MV;
    int direction = +1;  /* +1 while increasing voltage, -1 while decreasing. */

    request_voltage(pdo_index, voltage_mv);  /* Start at 5.00V PPS. */
    uint32_t last_step_ms = s_ms;

    while (1)
    {
        const uint32_t now_ms = s_ms;

        /* 1 short LED pulse for every PPS voltage request. */
        funDigitalWrite(LED_PIN,
            ((uint32_t)(now_ms - last_step_ms) < LED_PULSE_MS)
            ? LED_ON : LED_OFF);

        if ((uint32_t)(now_ms - last_step_ms) >= PPS_STEP_PERIOD_MS)
        {
            if (voltage_mv == PPS_MAX_VOLTAGE_MV)
            {
                direction = -1;
            }
            else if (voltage_mv == PPS_MIN_VOLTAGE_MV)
            {
                direction = +1;
            }

            if (direction > 0)
            {
                voltage_mv += PPS_STEP_MV;
            }
            else
            {
                voltage_mv -= PPS_STEP_MV;
            }

            request_voltage(pdo_index, voltage_mv);
            last_step_ms = s_ms;
        }
        Delay_Ms(10);
    }
}

static void systick_init_ms(void)
{
    SysTick->CTLR = 0;
    NVIC_EnableIRQ(SysTick_IRQn);
    uint64_t cnt = SysTick->CNTL | ((uint64_t)SysTick->CNTH << 32);
    cnt += (FUNCONF_SYSTEM_CORE_CLOCK / 1000U) - 1U;
    SysTick->CMPL = (uint32_t)(cnt & 0xFFFFFFFFU);
    SysTick->CMPH = (uint32_t)(cnt >> 32);
    s_ms = 0;
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE | SYSTICK_CTLR_STCLK;
}

void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
    uint64_t cnt = SysTick->CNTL | ((uint64_t)SysTick->CNTH << 32);
    cnt += (FUNCONF_SYSTEM_CORE_CLOCK / 1000U) - 1U;
    SysTick->CMPL = (uint32_t)(cnt & 0xFFFFFFFFU);
    SysTick->CMPH = (uint32_t)(cnt >> 32);
    SysTick->SR = 0;
    s_ms++;
}
