/* Native host-only tests: ring handling + CDC control; not hardware tests. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#define M5_HOST_TEST
#include "../m5_usb_cdc_echo.c"

static uint8_t result[1024];
static unsigned result_size;
static int busy_once;
int USBFS_SendEndpointNEW(int endpoint, uint8_t *data, int len, int copy)
{
    if (busy_once) { busy_once = 0; return -1; }
    if (endpoint != 3 || copy != 1 || len > 64) abort();
    if (result_size + (unsigned)len > sizeof(result)) abort();
    memcpy(result + result_size, data, (size_t)len);
    result_size += (unsigned)len;
    return 0;
}
static void require(int cond, const char *msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); }
}
int main(void)
{
    struct _USBState s = {0};
    uint8_t p[150];
    for (unsigned i=0; i<sizeof(p); ++i) p[i]=(uint8_t)i;
    HandleDataOut(&s, 2, p, 60);
    HandleDataOut(&s, 2, p+60, 60);
    HandleDataOut(&s, 2, p+120, 30);
    busy_once=1;
    M5_ServiceUsbEcho();
    require(result_size==0, "busy endpoint must not consume bytes");
    for (int i=0; i<4; ++i) M5_ServiceUsbEcho();
    require(result_size==150, "150 bytes must be returned");
    require(memcmp(result,p,150)==0, "binary echo must be identical");
    require(rx_overflows==0, "no overflow under normal flow");
    require(rx_read_idx==rx_write_idx, "RX queue empty after send");

    /* The fsusb.c handler contract: >0=data, 0=STALL, <0=ZLP ACK. */
    s.USBFS_SetupReqType = 0xA1; /* class/interface IN */
    s.USBFS_SetupReqLen = 7;
    require(HandleSetupCustom(&s, CDC_GET_LINE_CODING) == 7,
            "GET_LINE_CODING length");
    require(s.pCtrlPayloadPtr == cdc_line_coding &&
            s.pCtrlPayloadPtr[0] == 0x00 && s.pCtrlPayloadPtr[1] == 0xC2,
            "line rate 115200");

    /* fsusb.c leaves a pointer after a data-stage GET; next SETUP must clear it. */
    s.USBFS_SetupReqType = 0x21; /* class/interface OUT */
    s.USBFS_SetupReqLen = 0;
    require(HandleSetupCustom(&s, CDC_SET_CONTROL_LINE_STATE) < 0,
            "SET_CONTROL_LINE_STATE must ACK with ZLP, not STALL");
    require(s.pCtrlPayloadPtr == NULL,
            "new SETUP must clear stale EP0 payload pointer");
    require(HandleSetupCustom(&s, CDC_SEND_BREAK) < 0,
            "SEND_BREAK must ACK with ZLP");

    require(HandleSetupCustom(&s, 0x7f) == 0,
            "unsupported CDC request must STALL");
    s.USBFS_SetupReqType = 0xA1;
    s.USBFS_SetupReqLen = 0;
    require(HandleSetupCustom(&s, CDC_SET_CONTROL_LINE_STATE) == 0,
            "wrong-direction/length CDC request must STALL");

    uint8_t payload[7] = {0x80,0x25,0,0,0,0,8}; /* 9600 8N1 */
    s.USBFS_SetupReqType = 0x21;
    s.USBFS_SetupReqLen = 7;
    require(HandleSetupCustom(&s, CDC_SET_LINE_CODING) == 7,
            "SET_LINE_CODING accepted");
    HandleDataOut(&s, 0, payload, 7);
    require(cdc_line_coding[0] == 0x80 && cdc_line_coding[1] == 0x25,
            "line coding stored");
    require(s.USBFS_SetupReqLen == 0, "EP0 OUT completed");

    s.USBFS_SetupReqType = 0xA1;
    s.USBFS_SetupReqLen = 7;
    require(HandleSetupCustom(&s, CDC_GET_LINE_CODING) == 7,
            "GET_LINE_CODING after SET accepted");
    require(memcmp(s.pCtrlPayloadPtr, payload, sizeof(payload)) == 0,
            "GET_LINE_CODING returns updated coding");

    /* Deliberately overflow: 255 slots, additional bytes dropped safely. */
    uint8_t pattern[64];
    memset(pattern,0xA5,sizeof(pattern));
    for (int i=0; i<5; ++i) HandleDataOut(&s,2,pattern,64);
    require(rx_overflows>0, "overflow should be counted");
    for (int i=0; i<10; ++i) M5_ServiceUsbEcho();
    require(rx_read_idx==rx_write_idx, "drain queue after overflow");
    puts("PASS: host echo / USB busy / CDC controls / overflow");
    return 0;
}
