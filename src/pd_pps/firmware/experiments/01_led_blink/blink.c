#include "ch32fun.h"

// WeAct CH32X035F8U6 CoreBoard
// Onboard LED: PB12 (IC physical pin 15)
#define LED_PIN PB12

// LED polarity
#define LED_ON  FUN_HIGH
#define LED_OFF FUN_LOW

// Blink timing
#define BLINK_ON_MS   100
#define BLINK_OFF_MS  100
#define PAUSE_MS      500
#define BLINK_COUNT   5

int main(void)
{
    SystemInit();
    funGpioInitAll();

    // Configure LED pin as push-pull output
    funPinMode(
        LED_PIN,
        GPIO_Speed_10MHz | GPIO_CNF_OUT_PP
    );

    // Initial state: OFF
    funDigitalWrite(LED_PIN, LED_OFF);

    while (1)
    {
        // Blink 3 times
        for (int i = 0; i < BLINK_COUNT; i++)
        {
            // LED ON
            funDigitalWrite(LED_PIN, LED_ON);
            Delay_Ms(BLINK_ON_MS);

            // LED OFF
            funDigitalWrite(LED_PIN, LED_OFF);

            // Short pause between blinks
            if (i < BLINK_COUNT - 1)
            {
                Delay_Ms(BLINK_OFF_MS);
            }
        }

        // Long pause after 3 blinks
        Delay_Ms(PAUSE_MS);
    }
}
