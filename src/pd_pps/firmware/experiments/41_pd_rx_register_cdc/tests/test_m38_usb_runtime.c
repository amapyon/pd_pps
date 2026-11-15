#include "m38_usb.h"
#include "pd_phy_ch32x035.h"
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
static pd_stability_t diag_stats;
static pd_stop_diag_t diag_stop;
static pd_trace_t fake_trace[3] = {
    { .tick=0x12345678u, .value=0x00112233u, .aux=0x1234u, .kind=PDTR_TX_COMMAND, .detail=1u },
    { .tick=0x12345690u, .value=0x00000000u, .aux=0x0041u, .kind=PDTR_TX_RETRY, .detail=2u },
    { .tick=0x12345700u, .value=0x00000100u, .aux=0x0081u, .kind=PDTR_RX_GOODCRC, .detail=0u },
};
static unsigned peek_n;
/* Test double matches immutable, non-consuming FIFO snapshot semantics. */
bool phy_trace_snapshot(uint8_t index, pd_trace_t *out,
                        uint8_t *available, uint32_t *dropped) {
    peek_n++;
    if (available) *available = 3u;
    if (dropped) *dropped = 2u;
    if (!out || index >= 3u) return false;
    *out = fake_trace[index];
    return true;
}
static void issue(const char *cmd, const pe_t *p, uint32_t tick) {
    sent_n=0; sent[0]=0;
    HandleDataOut(&USBFSCTX,2,(uint8_t*)cmd,(int)strlen(cmd));
    for (unsigned i=0;i<15;i++) m38_usb_service(tick+i,p,false,&diag_stats,&diag_stop);
}
int main(void) {
    pe_t p={0};
    m38_usb_init(); assert(init_n == 1);
    USBFSCTX.USBFS_DevConfig=1;
    issue("PING\r\n",&p,1); assert(strcmp(sent,"PONG M41\r\n")==0);
    p.state=PE_READY; p.contract_valid=true; p.pps_active=true;
    p.requested_mv=7000; p.requested_ma=1500;
    issue("STAT\n",&p,50);
    assert(strstr(sent,"pe=4") && strstr(sent,"pps=1") && strstr(sent,"mv=7000"));
    m38_usb_service(100,&p,true,&diag_stats,&diag_stop);
    m38_usb_on_irq((uint16_t)(CRB_UIF_TRANSFER | (CUIS_TOKEN_SETUP<<12)));
    m38_usb_on_irq((uint16_t)(CRB_UIF_TRANSFER | (CUIS_TOKEN_IN<<12)));
    m38_usb_on_irq((uint16_t)(CRB_UIF_TRANSFER | (CUIS_TOKEN_OUT<<12)));
    m38_usb_on_bus_reset_irq();
    m38_usb_service(1100,&p,false,&diag_stats,&diag_stop);
    issue("DELTA\n",&p,1101);
    assert(strstr(sent,"valid=1") && strstr(sent,"ms=1000"));
    assert(strstr(sent,"set=1") && strstr(sent,"in=1") && strstr(sent,"out=1") && strstr(sent,"rst=1"));
    issue("COUNT\n",&p,1118);
    assert(strstr(sent,"rst=1") && strstr(sent,"irq=3"));
    /* Simulate the original PD17 snapshot values after a 10 s failure. */
    diag_stop.found = true;
    diag_stop.reason = PD_STOP_PE_TIMEOUT_ACCEPT;
    diag_stop.stop_rel_ms = 10123;
    diag_stop.pe_before = PE_WAIT_ACCEPT;
    diag_stop.pe_after = PE_RECOVERY;
    diag_stop.hr_rx_count = 1;
    issue("PDSTOP\n",&p,1125);
    assert(strstr(sent,"found=1 reason=6 ms=10123"));
    assert(strstr(sent,"before=2 after=6") && strstr(sent,"hr=1"));
    diag_stats.refresh_started = 2;
    diag_stats.refresh_completed = 1;
    diag_stats.pps_ready = 2;
    issue("PDSTAB\n",&p,1130);
    assert(strstr(sent,"ref_start=2 ref_done=1"));
    diag_stats.source_hr_rx = 1;
    diag_stats.recovery_entries = 1;
    diag_stats.vbus_loss = 0;
    diag_stats.adc_fault = 0;
    p.faults = 3;
    issue("PDERR\n",&p,1135);
    assert(strstr(sent,"hr=1 recovery=1 vbus_loss=0 adc_fault=0 faults=3"));
    issue("PDTINFO\n", &p, 1136);
    assert(strstr(sent, "count=3 capacity=127 dropped=2"));
    issue("PDTRACE 0\n", &p, 1137);
    assert(strstr(sent, "idx=0 count=3 tick=12345678 code=12 detail=1 aux=1234 value=00112233"));
    issue("PDTRACE 2\n", &p, 1138);
    assert(strstr(sent, "idx=2 count=3 tick=12345700 code=10"));
    issue("PDTRACE 3\n", &p, 1139);
    assert(strstr(sent, "idx=3 count=3 none=1"));
    issue("PDTRACE -1\n", &p, 1140);
    assert(strstr(sent, "ERR PDTRACE index"));
    issue("PDTRACE 127\n", &p, 1141);
    assert(strstr(sent, "ERR PDTRACE index"));
    issue("PDTRACE 0x1\n", &p, 1142);
    assert(strstr(sent, "ERR PDTRACE index"));
    issue("PDTRACE \n", &p, 1143);
    assert(strstr(sent, "ERR PDTRACE index"));
    issue("PDTRACE 1\n", &p, 1144);
    assert(strstr(sent, "idx=1 count=3 tick=12345690 code=13 detail=2 aux=0041"));
    assert(peek_n >= 5u);
    issue("NONSENSE\n",&p,1145);
    assert(strstr(sent,"ERR try PING STAT COUNT DELTA PDSTOP PDSTAB PDERR PDTINFO PDTRACE n"));
    puts("M41 host C CDC test PASS");
    return 0;
}
