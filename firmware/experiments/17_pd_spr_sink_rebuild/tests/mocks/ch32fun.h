#ifndef PD17_M12B_TEST_CH32FUN_H
#define PD17_M12B_TEST_CH32FUN_H
/* Minimal CH32X035 peripheral mock, host test only; NOT for device builds. */
#include <stdint.h>

#define FUNCONF_SYSTEM_CORE_CLOCK 48000000u

typedef struct {
    volatile uint32_t CNTL;
    volatile uint32_t CNTH;
} mock_systick_t;
typedef struct {
    volatile uint32_t APB2PCENR;
    volatile uint32_t AHBPCENR;
} mock_rcc_t;
typedef struct { volatile uint32_t CFGHR; } mock_gpio_t;
typedef struct { volatile uint32_t CTLR; } mock_afio_t;
typedef struct {
    volatile uint32_t DMA;
    volatile uint32_t CONFIG;
    volatile uint32_t STATUS;
    volatile uint32_t PORT_CC1;
    volatile uint32_t PORT_CC2;
    volatile uint32_t BMC_CLK_CNT;
    volatile uint32_t CONTROL;
    volatile uint32_t TX_SEL;
    volatile uint32_t BMC_TX_SZ;
    volatile uint32_t BMC_BYTE_CNT;
} mock_usbpd_t;

extern mock_systick_t mock_systick;
extern mock_rcc_t mock_rcc;
extern mock_gpio_t mock_gpioc;
extern mock_afio_t mock_afio;
extern mock_usbpd_t mock_usbpd;

#define SysTick (&mock_systick)
#define RCC (&mock_rcc)
#define GPIOC (&mock_gpioc)
#define AFIO (&mock_afio)
#define USBPD (&mock_usbpd)

#define USBPD_IRQn 44
#define RCC_APB2Periph_GPIOC 0x00000001u
#define RCC_APB2Periph_AFIO  0x00000002u
#define RCC_USBPD 0x00000004u
#define GPIO_Speed_10MHz 0x1u
#define GPIO_CNF_OUT_OD   0x4u
#define USBPD_IN_HVT 0x0001u
#define USBPD_PHY_V33 0x0002u
#define IE_RX_ACT   0x0001u
#define IE_RX_RESET 0x0002u
#define IE_TX_END   0x0004u
#define PD_DMA_EN   0x0008u
#define PD_FILT_EN  0x0010u
#define BUF_ERR     0x0020u
#define IF_RX_BIT   0x0040u
#define IF_RX_BYTE  0x0080u
#define IF_RX_ACT   0x0001u
#define IF_RX_RESET 0x0002u
#define IF_TX_END   0x0004u
#define CC_CMP_MASK 0x000fu
#define CC_CMP_66   0x0003u
#define PA_CC_AI    0x0100u
#define CC_SEL      0x1000u
#define UPD_TMR_RX 0x21u
#define UPD_TMR_TX 0x23u
#define UPD_SOP0    0x00u
#define PD_TX_EN    0x0001u
#define BMC_START   0x0002u
#define BMC_AUX_MASK 0xf000u
#define BMC_AUX_SOP0 0u

void NVIC_DisableIRQ(int irq);
void NVIC_EnableIRQ(int irq);
void Delay_Us(uint32_t delay);
#endif
