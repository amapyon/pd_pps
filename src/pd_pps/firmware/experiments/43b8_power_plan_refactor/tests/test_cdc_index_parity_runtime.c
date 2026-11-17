/* CDC wire-format outputs compared against the pristine M43B-5 source. */
#include "m38_usb.h"
#include "pd_phy_ch32x035.h"
#include "fsusb.h"
#include "m42_diag.h"
#include "pd_vbus_monitor.h"
#include "pd_sensors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

void HandleDataOut(struct _USBState*, int, uint8_t*, int);
struct _USBState USBFSCTX;
static uint8_t epbuf[64];
static char sent[256];
static unsigned sent_n;
int USBFSSetup(void) { return 0; }
uint8_t *USBFS_GetEPBufferIfAvailable(int ep) { assert(ep == 3); return epbuf; }
int USBFS_SendEndpoint(int ep, int len) {
    assert(ep == 3); assert(len >= 0 && len <= 60);
    assert(sent_n + (unsigned)len < sizeof(sent));
    memcpy(sent+sent_n,epbuf,(size_t)len);
    sent_n+=(unsigned)len; sent[sent_n]=0;
    return 0;
}
static pd_vbus_history_t hist;
const pd_vbus_history_t *pd_vbus_monitor_history(void) { return &hist; }
uint32_t pd_vbus_monitor_timeouts(void) { return 0u; }
bool pd_vbus_monitor_stopped_on_error(void) { return false; }
bool pd_vbus_monitor_loss_detected(void) { return false; }
uint32_t adc_voltage_vbus_mv(uint16_t raw) { return (uint32_t)raw*9u; }
const pd_sensor_reading_t *pd_sensors_ready_reading(void) { return NULL; }
bool phy_trace_snapshot(uint8_t index, pd_trace_t *out,
                        uint8_t *available, uint32_t *dropped) {
    if (available) *available=3u;
    if (dropped) *dropped=0u;
    if (out && index < 3u) {
        *out=(pd_trace_t){.tick=100u+index,.kind=PDTR_RX_GOODCRC,.detail=index};
        return true;
    }
    return false;
}
static void issue(const char *command, const pe_t *p, uint32_t now)
{
    char req[64];
    size_t len=strlen(command);
    assert(len + 1u < sizeof(req));
    memcpy(req,command,len); req[len++]='\n';req[len]=0;
    sent_n=0;sent[0]=0;
    HandleDataOut(&USBFSCTX,2,(uint8_t*)req,(int)len);
    for (unsigned i=0; i<20; i++) m38_usb_service(now+i,p,false,NULL,NULL);
    printf("%s => %s",command,sent);
}
int main(void)
{
    pe_t p={0};
    m38_usb_init();USBFSCTX.USBFS_DevConfig=1;
    const char *cases[]={
        "VBLOG 0", "VBLOG 1", "VBLOG 39", "VBLOG 40", "VBLOG -1",
        "VBLOG 000", "VBLOG 039", "VBLOG 01x", "VBLOG ",
        "VBLOG 4294967296", "VBLOG 99999999", "VBLOG +1",
        "PDTRACE 0", "PDTRACE 1", "PDTRACE 2", "PDTRACE 3",
        "PDTRACE 126", "PDTRACE 127", "PDTRACE -1",
        "PDTRACE 000", "PDTRACE 0126", "PDTRACE 01x", "PDTRACE ",
        "PDTRACE 4294967296", "PDTRACE 99999999", "PDTRACE +1",
        "PING", "COUNT", "STAT", "DELTA", "PDTINFO", "INA", "NONCMD"
    };
    for (unsigned i=0; i<sizeof(cases)/sizeof(cases[0]); i++)
        issue(cases[i],&p,100u+i*50u);
    return 0;
}
