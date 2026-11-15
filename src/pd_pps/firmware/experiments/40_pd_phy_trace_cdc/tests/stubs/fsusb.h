#ifndef M38_HOST_FSUSB_H
#define M38_HOST_FSUSB_H
#include <stdint.h>
#define CRB_UIF_TRANSFER 0x02u
#define CMASK_UIS_TOKEN 0x3000u
#define CUIS_TOKEN_SETUP 3
#define CUIS_TOKEN_IN 2
#define CUIS_TOKEN_OUT 0
#define CDC_SET_LINE_CODING 0x20
#define CDC_GET_LINE_CODING 0x21
#define CDC_SEND_BREAK 0x23
struct _USBState {
    uint8_t USBFS_DevConfig;
    uint16_t USBFS_SetupReqLen;
    uint8_t USBFS_SetupReqType;
    const uint8_t *pCtrlPayloadPtr;
};
extern struct _USBState USBFSCTX;
int USBFSSetup(void);
uint8_t *USBFS_GetEPBufferIfAvailable(int ep);
int USBFS_SendEndpoint(int ep,int len);
#endif
