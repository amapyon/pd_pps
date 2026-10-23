/*
 * WeAct CH32X035F8U6 CoreBoard
 * USB PD Sink: select a 9 V FIXED supply, if advertised.
 *
 * Derived from the ch32fun examples_x035/usbpd_sink sample supplied
 * by the user (Bogdan Ionescu, 2025-06-21).
 *
 * LED: PB12 (IC physical pin 15); slow, countable status blinks.
 * IMPORTANT: three flashes mean that a request was issued. They do
 * not independently verify the VBUS voltage.
 */

#include "ch32fun.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define USBPD_IMPLEMENTATION
#include "../ch32fun/examples_x035/usbpd_sink/usbpd.h"

#define LED_PIN             PB12
#define LED_ON              FUN_HIGH
#define LED_OFF             FUN_LOW
#define TARGET_MV           9000U
#define MIN_PDO_CURRENT_MA  500U
#define NEGOTIATION_TIMEOUT_MS 20000U

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

static void led_flash(uint8_t times, uint32_t on_ms,
                      uint32_t between_ms, uint32_t after_ms)
{
    for (uint8_t i = 0; i < times; ++i)
    {
        funDigitalWrite(LED_PIN, LED_ON);
        Delay_Ms(on_ms);
        funDigitalWrite(LED_PIN, LED_OFF);
        if (i + 1U < times)
            Delay_Ms(between_ms);
    }
    Delay_Ms(after_ms);
}

/*
 * Source PDO, Fixed Supply (USB PD):
 *   bits 31:30: PDO type (0 = Fixed)
 *   bits 19:10: voltage, units of 50 mV
 *   bits  9: 0: maximum current, units of 10 mA
 *
 * Decode bytes rather than depending on undocumented C bitfield names.
 * The RISC-V MCU uses little-endian byte order.
 */
static uint32_t source_pdo_raw(const USBPD_SourcePDO_t *pdo)
{
    const uint8_t *p = (const uint8_t *)pdo;
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int find_fixed_9v_pdo(void)
{
    USBPD_SPR_CapabilitiesMessage_t *capabilities = NULL;
    const size_t count = USBPD_GetCapabilities(&capabilities);
    if (!capabilities || !count)
        return -1;

    for (size_t i = 0; i < count; ++i)
    {
        const USBPD_SourcePDO_t *pdo = &capabilities->Source[i];
        if (pdo->Header.PDOType != eUSBPD_PDO_FIXED)
            continue;

        const uint32_t raw = source_pdo_raw(pdo);
        const uint32_t voltage_mv = ((raw >> 10) & 0x3FFU) * 50U;
        const uint32_t max_current_ma = (raw & 0x3FFU) * 10U;

        if (voltage_mv == TARGET_MV && max_current_ma >= MIN_PDO_CURRENT_MA)
            return (int)i;  /* USBPD_SelectPDO takes a zero-based index. */
    }
    return -1;
}

int main(void)
{
    SystemInit();
    led_init();
    systick_init_ms();

    /* Debug output is optional. With no WCH-LinkE attached, keep running. */
    debugger_attached = !WaitForDebuggerToAttach(300);
    LOG("BOOT: CH32X035 PD 9V diagnostic");

    /* Same peripheral setup as the ch32fun USB PD Sink example. */
    RCC->APB2PCENR |= (RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA);

    /* WeAct CoreBoard MCU is powered from its 3.3 V rail. */
    LOG("PD: init, VDD=3.3V");
    USBPD_Result_e result = USBPD_Init(eUSBPD_VCC_3V3);
    LOG("PD: init result = %s", USBPD_ResultToStr(result));
    if (result != eUSBPD_OK)
    {
        while (1)
            led_flash(5, 300, 300, 2500);    /* Fatal initialization error */
    }

    for (;;)
    {
        USBPD_Reset();
        LOG("PD: negotiate start");
        const uint32_t begin_ms = s_ms;

        /* Complete the library's initial Sink negotiation. */
        do
        {
            result = USBPD_SinkNegotiate();
            if ((uint32_t)(s_ms - begin_ms) > NEGOTIATION_TIMEOUT_MS)
                break;
        } while (result == eUSBPD_BUSY);

        LOG("PD: negotiate result=%s state=%s", 
            USBPD_ResultToStr(result), USBPD_StateToStr(USBPD_GetState()));
        if (result != eUSBPD_OK)
        {
            led_flash(5, 300, 300, 2500); /* Communication/timeout error */
            continue;
        }

        USBPD_SPR_CapabilitiesMessage_t *caps = NULL;
        size_t caps_count = USBPD_GetCapabilities(&caps);
        LOG("PD: source PDO count=%u", (unsigned)caps_count);
        for (size_t i = 0; caps && i < caps_count; ++i)
        {
            uint32_t raw = source_pdo_raw(&caps->Source[i]);
            unsigned type = (unsigned)((raw >> 30) & 3U);
            unsigned mv = type == 0U ? (unsigned)(((raw >> 10) & 0x3ffU) * 50U) : 0U;
            unsigned ma = type == 0U ? (unsigned)((raw & 0x3ffU) * 10U) : 0U;
            LOG("PD: PDO[%u] type=%u raw=0x%08lx fixed=%umV %umA",
                (unsigned)i, type, (unsigned long)raw, mv, ma);
        }

        const int pdo_index = find_fixed_9v_pdo();
        LOG("PD: 9V fixed PDO index=%d", pdo_index);
        if (pdo_index < 0)
        {
            /* No 9 V FIXED PDO: do not request a higher voltage. */
            while (1)
                led_flash(2, 300, 300, 2500);
        }

        /* The sample uses (PDO index, 0) for a Fixed PDO. */
        LOG("PD: select PDO index=%d start", pdo_index);
        /* Do not assume this API call proves a 9V contract. */
        (void)USBPD_SelectPDO((size_t)pdo_index, 0);
        LOG("PD: select PDO returned, state=%s", USBPD_StateToStr(USBPD_GetState()));

        /* No further USBPD_Reset here: that would undo the selection. */
        while (1)
            led_flash(3, 300, 300, 2500);
    }
}

/* Millisecond timebase copied in structure from the supplied sample. */
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
