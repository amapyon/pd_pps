#ifndef M35_TEST_CH32FUN_H
#define M35_TEST_CH32FUN_H
#include <stdint.h>
typedef struct { uint8_t BASE_CTRL, UDEV_CTRL, INT_EN; } usbfs_regs_t;
typedef struct { uint32_t CTLR; } afio_regs_t;
typedef struct { uint32_t CFGXR; } gpio_regs_t;
extern usbfs_regs_t USBFS_REGS;
extern afio_regs_t AFIO_REGS;
extern gpio_regs_t GPIOC_REGS;
#define USBFS (&USBFS_REGS)
#define AFIO (&AFIO_REGS)
#define GPIOC (&GPIOC_REGS)
#define USB_IOEN (1u<<7)
#define UDP_PUE_MASK (3u<<2)
#define UDM_PUE_MASK (3u)
#define GPIO_CFGLR_IN_FLOAT 4u
#define USB_IRQn 45
#define USBFS_UD_PD_DIS 0x80u
void NVIC_DisableIRQ(int irq);
#endif
