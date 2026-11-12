#ifndef M8E_MOCK_IRQ_USB_H
#define M8E_MOCK_IRQ_USB_H
#include <stdint.h>
typedef struct {
    uint8_t INT_EN, UEP2_3_MOD, BASE_CTRL, UDEV_CTRL, RX_LEN;
    uint8_t ep3_len, ep3_ctrl;
} M8E_TestUsbRegs;
typedef struct {uint8_t busy,mode; } M8E_TestEndpoint;
typedef struct {
    M8E_TestEndpoint endpoints[4];
    uint16_t USBFS_SetupReqLen;
    uint8_t USBFS_errata_dont_send_endpoint_in_window;
} M8E_TestUsbContext;
extern M8E_TestUsbRegs m8e_test_regs;
extern M8E_TestUsbContext USBFSCTX;
#define USBFS (&m8e_test_regs)
#define UEP_CTRL_LEN(n) (m8e_test_regs.ep3_len)
#define UEP_CTRL_TX(n) (m8e_test_regs.ep3_ctrl)
#endif
