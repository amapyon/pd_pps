/*
 * 04_pps_6v - WeAct CH32X035F8U6 CoreBoard
 * USB Power Delivery Sink: request 6.00 V using a PPS APDO.
 *
 * Based on the usbpd_sink.c example supplied by the user
 * (Bogdan Ionescu, 2025-06-21) and the 03_pd_9v project.
 *
 * PB12 (IC pin 15): status LED.
 * The LED indicates a request was issued; it does NOT measure VBUS.
 * Always check VBUS with a USB PD meter, with no load initially.
 */

#include "ch32fun.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define USBPD_IMPLEMENTATION
#include "../ch32fun/examples_x035/usbpd_sink/usbpd.h"

#define LED_PIN                PB12
#define LED_ON                 FUN_HIGH
#define LED_OFF                FUN_LOW

#define PPS_VOLTAGE_MV         6000U
#define PPS_VOLTAGE_100MV     (PPS_VOLTAGE_MV / 100U)
#define PPS_MIN_CURRENT_MA    500U  /* Minimum capability required, not an RDO current limit. */
#define PPS_REFRESH_MS        5000U /* PPS contracts need periodic requests. */
#define NEGOTIATE_TIMEOUT_MS  20000U

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

/* Extract the 32-bit PDO from the library's in-memory representation. */
static uint32_t pdo_raw(const USBPD_SourcePDO_t *pdo)
{
    const uint8_t *p = (const uint8_t *)pdo;
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/*
 * PPS APDO fields:
 *  bits 31:30 = 3 (Augmented PDO)
 *  bits 29:28 = 0 (SPR PPS)
 *  bits 24:17 = maximum voltage / 100 mV
 *  bits 15: 8 = minimum voltage / 100 mV
 *  bits  6: 0 = maximum current / 50 mA
 * Returns the zero-based PDO index, or -1 if 6.00 V is unsupported.
 */
static int find_6v_pps_pdo(void)
{
    USBPD_SPR_CapabilitiesMessage_t *caps = NULL;
    size_t count = USBPD_GetCapabilities(&caps);
    if (!caps || count == 0) return -1;

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

        const uint32_t min_100mv = (raw >> 8) & 0xFFU;
        const uint32_t max_100mv = (raw >> 17) & 0xFFU;
        const uint32_t max_ma = (raw & 0x7FU) * 50U;

        LOG("PD: PDO[%u] PPS %lu-%lu mV, max %lu mA, raw=0x%08lx",
            (unsigned)i, (unsigned long)(min_100mv * 100U),
            (unsigned long)(max_100mv * 100U), (unsigned long)max_ma,
            (unsigned long)raw);

        if (min_100mv <= PPS_VOLTAGE_100MV &&
            PPS_VOLTAGE_100MV <= max_100mv &&
            max_ma >= PPS_MIN_CURRENT_MA)
        {
            return (int)i;
        }
    }
    return -1;
}

/* Non-blocking LED indication: 3 flashes within every 3 seconds. */
static void led_show_pps_requested(uint32_t now_ms)
{
    const uint32_t phase = now_ms % 3000U;
    const bool on = (phase < 200U) ||
                    (phase >= 400U && phase < 600U) ||
                    (phase >= 800U && phase < 1000U);
    funDigitalWrite(LED_PIN, on ? LED_ON : LED_OFF);
}

int main(void)
{
    SystemInit();
    led_init();
    systick_init_ms();

    debugger_attached = !WaitForDebuggerToAttach(300);
    LOG("BOOT: CH32X035 USB PD PPS 6.00V");

    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA;

    USBPD_Result_e result = USBPD_Init(eUSBPD_VCC_3V3);
    LOG("PD: init=%s", USBPD_ResultToStr(result));
    if (result != eUSBPD_OK)
    {
        LOG("ERROR: PD init failed");
        led_error_forever(5);
    }

    USBPD_Reset();
    const uint32_t negotiate_start_ms = s_ms;
    do
    {
        result = USBPD_SinkNegotiate();
        if ((uint32_t)(s_ms - negotiate_start_ms) >= NEGOTIATE_TIMEOUT_MS)
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

    const int pdo_index = find_6v_pps_pdo();
    if (pdo_index < 0)
    {
        LOG("ERROR: no PPS PDO supports 6.00V");
        led_error_forever(2);
    }

    LOG("PPS: request 6000mV, PDO index=%d, API voltage=%u (100mV units)",
        pdo_index, (unsigned)PPS_VOLTAGE_100MV);

    /*
     * ch32fun USBPD_SelectPDO() takes the zero-based PDO index and,
     * for PPS, the voltage in units of 100 mV (as in its sample).
     * Its signature does not expose a desired current parameter.
     */
    (void)USBPD_SelectPDO((size_t)pdo_index, PPS_VOLTAGE_100MV);
    LOG("PPS: selection returned, state=%s", USBPD_StateToStr(USBPD_GetState()));

    /*
     * Refresh the PPS request every 5 seconds. No USBPD_Reset() here:
     * resetting would discard the negotiated contract.
     */
    uint32_t last_request_ms = s_ms;
    while (1)
    {
        const uint32_t now_ms = s_ms;
        led_show_pps_requested(now_ms);
        if ((uint32_t)(now_ms - last_request_ms) >= PPS_REFRESH_MS)
        {
            (void)USBPD_SelectPDO((size_t)pdo_index, PPS_VOLTAGE_100MV);
            LOG("PPS: refresh 6000mV, state=%s",
                USBPD_StateToStr(USBPD_GetState()));
            last_request_ms = s_ms;
        }
        Delay_Ms(20);
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
