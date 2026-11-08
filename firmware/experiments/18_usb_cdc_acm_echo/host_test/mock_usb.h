#ifndef M5_MOCK_USB_H
#define M5_MOCK_USB_H
#include <stdint.h>
struct _USBState {
    uint8_t USBFS_SetupReqType;
    uint16_t USBFS_SetupReqLen;
    uint8_t *pCtrlPayloadPtr;
};
int USBFS_SendEndpointNEW(int endpoint, uint8_t *data, int len, int copy);
#endif
