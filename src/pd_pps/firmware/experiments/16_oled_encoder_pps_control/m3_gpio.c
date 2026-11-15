#include "ch32fun.h"
#include "m3_gpio.h"

/* Input pull-ups: switch contacts must short signal to board GND only.
 * Enable pull-up by setting GPIO output data bit HIGH in input PUPD mode.
 * Do not use 5V, PPS VBUS or an external output drive on these lines.
 */
static void configure_input_pullup(uint32_t pin)
{
    funDigitalWrite(pin, FUN_HIGH);
    funPinMode(pin, GPIO_CNF_IN_PUPD);
}

void m3_gpio_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA;
    configure_input_pullup(M3_PIN_EC11_A);
    configure_input_pullup(M3_PIN_EC11_B);
    configure_input_pullup(M3_PIN_EC11_SW);
    configure_input_pullup(M3_PIN_BACK);
}

m3_levels_t m3_gpio_read(void)
{
    m3_levels_t v;
    v.a_high = (funDigitalRead(M3_PIN_EC11_A) != 0);
    v.b_high = (funDigitalRead(M3_PIN_EC11_B) != 0);
    v.encoder_sw_high = (funDigitalRead(M3_PIN_EC11_SW) != 0);
    v.back_high = (funDigitalRead(M3_PIN_BACK) != 0);
    return v;
}
