#ifndef M3_GPIO_H
#define M3_GPIO_H

#include "m3_input.h"

/* Experimental GPIO assignments on WeAct CH32X035 CoreBoard.
 * All are accessible at bottom header A4..A7. No PD/INA226/OLED pins used.
 * This is NOT a final product pin allocation; confirm module pins before wiring.
 */
#define M3_PIN_EC11_A   PA4
#define M3_PIN_EC11_B   PA5
#define M3_PIN_EC11_SW  PA6
#define M3_PIN_BACK     PA7

void m3_gpio_init(void);
m3_levels_t m3_gpio_read(void);

#endif
