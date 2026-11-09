#ifndef M6_MOCK_USB_H
#define M6_MOCK_USB_H
#include <stdint.h>
struct _USBState {
    uint8_t USBFS_SetupReqType;
    uint16_t USBFS_SetupReqLen;
    uint8_t *pCtrlPayloadPtr;
};
uint8_t *USBFS_GetEPBufferIfAvailable(int endpoint);
int USBFS_SendEndpoint(int endpoint, int len);
#endif
