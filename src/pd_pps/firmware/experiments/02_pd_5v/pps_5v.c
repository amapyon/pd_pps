#include "ch32fun.h"

/*
 * WeAct CH32X035F8U6 CoreBoard: USB-C default 5 V VBUS test
 *
 * IMPORTANT:
 * - This program does NOT generate 5 V or send a USB PD request.
 * - A USB-C source supplies default 5 V VBUS after it detects a valid sink
 *   termination (Rd) on the CC line.
 * - The board's USB-C CC termination and power circuitry must be correct.
 * - Verify VBUS-to-GND with a meter and do not attach a load initially.
 * - Do not connect 5 V to the MCU 3V3 or GPIO pins.
 *
 * LED: PB12 (CH32X035F8U6 QFN20 IC physical pin 15)
 */

#define LED_PIN          PB12
#define LED_ON           FUN_HIGH
#define LED_OFF          FUN_LOW
#define FLASH_ON_MS      100
#define FLASH_OFF_MS     100
#define PAUSE_MS         500
#define FLASH_COUNT      3

int main(void)
{
    SystemInit();
    funGpioInitAll();

    funPinMode(LED_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funDigitalWrite(LED_PIN, LED_OFF);

    /* Deliberately do not negotiate USB PD: VBUS remains at default 5 V. */
    while (1)
    {
        for (int i = 0; i < FLASH_COUNT; ++i)
        {
            funDigitalWrite(LED_PIN, LED_ON);
            Delay_Ms(FLASH_ON_MS);

            funDigitalWrite(LED_PIN, LED_OFF);
            if (i + 1 < FLASH_COUNT)
            {
                Delay_Ms(FLASH_OFF_MS);
            }
        }
        Delay_Ms(PAUSE_MS);
    }
}
