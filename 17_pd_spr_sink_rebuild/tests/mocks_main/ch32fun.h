#ifndef PD17_M13A2_MAIN_MOCK_H
#define PD17_M13A2_MAIN_MOCK_H
#include <stdint.h>
#define FUNCONF_SYSTEM_CORE_CLOCK 48000000u
#define SYSTICK_CTLR_STE 1u
#define SYSTICK_CTLR_STCLK 2u
typedef struct { volatile uint32_t CNTL, CNTH, CTLR; } mock_main_systick_t;
extern mock_main_systick_t mock_main_systick;
#define SysTick (&mock_main_systick)
void SystemInit(void);
#endif
