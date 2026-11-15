#include "m38_usb.h"
#include "fsusb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

void HandleDataOut(struct _USBState *, int, uint8_t *, int);
int HandleSetupCustom(struct _USBState *, int);
struct _USBState USBFSCTX;
static uint8_t epbuf[64];
static char sent[256];
static unsigned sent_n;
static unsigned init_n;
int USBFSSetup(void) { ++init_n; return 0; }
uint8_t *USBFS_GetEPBufferIfAvailable(int ep) { assert(ep == 3); return epbuf; }
int USBFS_SendEndpoint(int ep,int len) {
    assert(ep == 3); assert(len >= 0 && len <= 60);
    assert(sent_n + (unsigned)len < sizeof(sent));
    memcpy(sent + sent_n, epbuf, (size_t)len);
    sent_n += (unsigned)len; sent[sent_n] = 0;
    return 0;
}
static void issue(const char *cmd, const pe_t *p, uint32_t tick) {
    sent_n=0; sent[0]=0;
    HandleDataOut(&USBFSCTX,2,(uint8_t*)cmd,(int)strlen(cmd));
    for (unsigned i=0;i<15;i++) m38_usb_service(tick+i,p,false);
}
int main(void) {
    pe_t p={0};
    m38_usb_init(); assert(init_n == 1);
    USBFSCTX.USBFS_DevConfig=1;
    issue("PING\r\n",&p,1); assert(strcmp(sent,"PONG M38A\r\n")==0);
    p.state=PE_READY; p.contract_valid=true; p.pps_active=true;
    p.requested_mv=7000; p.requested_ma=1500;
    issue("STAT\n",&p,50);
    assert(strstr(sent,"pe=4") && strstr(sent,"pps=1") && strstr(sent,"mv=7000"));
    m38_usb_service(100,&p,true);
    m38_usb_on_irq((uint16_t)(CRB_UIF_TRANSFER | (CUIS_TOKEN_SETUP<<12)));
    m38_usb_on_irq((uint16_t)(CRB_UIF_TRANSFER | (CUIS_TOKEN_IN<<12)));
    m38_usb_on_irq((uint16_t)(CRB_UIF_TRANSFER | (CUIS_TOKEN_OUT<<12)));
    m38_usb_on_bus_reset_irq();
    m38_usb_service(1100,&p,false);
    issue("DELTA\n",&p,1101);
    assert(strstr(sent,"valid=1") && strstr(sent,"ms=1000"));
    assert(strstr(sent,"set=1") && strstr(sent,"in=1") && strstr(sent,"out=1") && strstr(sent,"rst=1"));
    issue("COUNT\n",&p,1118);
    assert(strstr(sent,"rst=1") && strstr(sent,"irq=3"));
    issue("NONSENSE\n",&p,1140);
    assert(strstr(sent,"ERR try PING STAT COUNT DELTA"));
    puts("M38A host C CDC test PASS");
    return 0;
}
