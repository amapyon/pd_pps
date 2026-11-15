/* M38B: USBPD-disabled comparison for M38A.
 * External CC 5.1k Rd resistors remain physically connected.
 * This firmware neither requests >5V nor runs PD Policy Engine / USBPD ISR.
 */
#include <stdbool.h>
#include <stdint.h>
#include "ch32fun.h"
#include "m38b_usb.h"

static uint32_t now_ms(void)
{
    uint32_t hi1, hi2, lo;
    do {
        hi1 = SysTick->CNTH;
        lo = SysTick->CNTL;
        hi2 = SysTick->CNTH;
    } while (hi1 != hi2);
    uint64_t cycles = ((uint64_t)hi1 << 32) | lo;
    return (uint32_t)(cycles / (FUNCONF_SYSTEM_CORE_CLOCK / 1000u));
}

int main(void)
{
    SystemInit();
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STCLK;
    /* USBFS only: intentionally no PD stack or contract negotiation. */
    m38b_usb_init();
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOB;
    funPinMode(PB12, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
    funDigitalWrite(PB12, FUN_LOW);
    uint32_t last_led_ms = now_ms();
    bool led_on = false;
    while (1) {
        uint32_t now = now_ms();
        if ((uint32_t)(now - last_led_ms) >= 250u) {
            last_led_ms = now;
            led_on = !led_on;
            funDigitalWrite(PB12, led_on ? FUN_HIGH : FUN_LOW);
        }
        m38b_usb_service();
    }
}
