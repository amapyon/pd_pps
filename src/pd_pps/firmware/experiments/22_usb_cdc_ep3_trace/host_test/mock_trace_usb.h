#ifndef M8D_MOCK_TRACE_USB_H
#define M8D_MOCK_TRACE_USB_H
#include <stdint.h>
typedef struct {
    uint8_t INT_FG, INT_ST, BASE_CTRL, UDEV_CTRL, INT_EN, UEP2_3_MOD;
    uint8_t EP3_TX_LEN, EP3_TX_CTRL;
} MockUSBRegs;
extern MockUSBRegs mock_regs;
#define USBFS (&mock_regs)
#define UEP_CTRL_LEN(n) (mock_regs.EP3_TX_LEN)
#define UEP_CTRL_TX(n) (mock_regs.EP3_TX_CTRL)
typedef struct { volatile uint8_t busy; uint8_t mode; } MockEndpoint;
typedef struct {
    MockEndpoint endpoints[4];
    uint16_t USBFS_SetupReqLen;
    uint8_t USBFS_errata_dont_send_endpoint_in_window;
} MockUSBState;
extern MockUSBState USBFSCTX;
#endif
