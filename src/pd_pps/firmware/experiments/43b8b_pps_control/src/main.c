#include <stdbool.h>
#include <stdint.h>
#include "ch32fun.h"
#include "pd_session.h"
#include "m38_usb.h"

/* M43B-7 P2: top-level ordering only. PD state, safety and reporting live
 * in pd_session.c and pd_session_diag.c. PHY/USB ISRs are not changed. */
static uint32_t now_ms(void) {
    uint32_t hi1, hi2, lo;
    do { hi1=SysTick->CNTH; lo=SysTick->CNTL; hi2=SysTick->CNTH; } while (hi1 != hi2);
    uint64_t cycles = ((uint64_t)hi1 << 32) | lo;
    return (uint32_t)(cycles / (FUNCONF_SYSTEM_CORE_CLOCK / 1000u));
}

int main(void) {
    SystemInit();
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STCLK;
    pd_session_init_core();
    /* The original precedence is USBPD=0x00, USBFS=0x80.
     * Both priorities are set before m38_usb_init enables USBFS. */
    NVIC_SetPriority(USBPD_IRQn, 0x00u);
    NVIC_SetPriority(USBFS_IRQn, 0x80u);
    m38_usb_init();
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOB;
    funPinMode(PB12, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
    funDigitalWrite(PB12, FUN_LOW);
    pd_session_init_state();
    uint32_t m29_led_last_ms = now_ms();
    bool m29_led_on = false;
    for (;;) {
        uint32_t now = now_ms();
        if ((uint32_t)(now - m29_led_last_ms) >= 250u) {
            m29_led_last_ms = now;
            m29_led_on = !m29_led_on;
            funDigitalWrite(PB12, m29_led_on ? FUN_HIGH : FUN_LOW);
        }
        pd_session_step(now);
    }
}
