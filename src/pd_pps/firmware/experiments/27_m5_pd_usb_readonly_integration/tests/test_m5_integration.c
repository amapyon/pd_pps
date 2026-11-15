#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include "../src/m5_bridge_runtime.h"
#include "../src/m5_usb_io.h"
#include "../src/m5_recovery_nb.h"

static unsigned passed;
#define PASS(title) do {printf("%02u PASS %s\n",++passed,title);} while(0)
static pe_t pps_ready(void) {
    pe_t p={0};
    p.state=PE_READY; p.contract_valid=true;p.pps_active=true;
    p.caps_fresh=true;p.caps_epoch=1;p.caps.count=6;
    p.caps.pdo[0]=0x0801912cu;
    p.caps.pdo[1]=0x0002d12cu;
    p.caps.pdo[2]=0x0003c12cu;
    p.caps.pdo[3]=0x0004b12cu;
    p.caps.pdo[4]=0x0006412cu;
    p.caps.pdo[5]=0xc1a4213cu;
    p.pdo_index=6;p.requested_mv=7000;p.requested_ma=1000;
    return p;
}
static pd_sensor_reading_t reading(void) {
    pd_sensor_reading_t r={0};
    r.ina_ok=true;r.ina_bus_mv=6965;r.ina_current_ua=-50123;r.ina_power_mw=-349;
    r.adc_ok=true;r.adc_calibrated_range=true;r.adc_cal_mv=7001;
    return r;
}
static void send(m5_session_t *s,const char *line) {
    for(size_t i=0;line[i];i++)assert(m5_session_rx(s,(uint8_t)line[i]));
}
static void get(m5_session_t *s,const char *expect) {
    const char *out=NULL;size_t n=m5_session_tx_peek(s,&out);
    assert(out&&n==strlen(expect)&&n<=63);
    if(memcmp(out,expect,n)) {
        fprintf(stderr,"EXPECT: [%s] GOT: [%.*s]\n",expect,(int)n,out);assert(0);
    }
    m5_session_tx_accepted(s,n);
    assert(!m5_session_has_reply(s));
}
static void check(m5_session_t *s,const char *req,const char *resp) {
    send(s,req);get(s,resp);
}
typedef struct {
    const char *in;
    size_t cursor;
    char tx[64];size_t tx_len;
    unsigned fail_sends;
} fake_io_t;
static bool fake_rx(void *ctx,uint8_t *out){
    fake_io_t *f=(fake_io_t*)ctx;
    if(!f->in[f->cursor])return false;
    *out=(uint8_t)f->in[f->cursor++];return true;
}
static int fake_tx(void *ctx,const uint8_t *b,size_t n){
    fake_io_t *f=(fake_io_t*)ctx;
    if(f->fail_sends){f->fail_sends--;return -1;}
    assert(n<=63&&f->tx_len+n<sizeof(f->tx));
    memcpy(f->tx+f->tx_len,b,n);f->tx_len+=n;
    f->tx[f->tx_len]=0;
    return 0;
}
static void io_test(m5_session_t *ss,m5_usb_health_t *h,pe_t *pe,
                    pd_sensor_reading_t *rd){
    m5_session_usb_reset(ss);
    assert(m5_bridge_publish_pd17(ss,pe,rd,true,h));
    fake_io_t f={"GET_CAP 6\n",0,"",0,0};
    m5_usb_io_t io={fake_rx,fake_tx,&f};
    for(unsigned i=0;i<20;i++)m5_usb_io_poll(ss,&io,false);
    assert(!strcmp(f.tx,"CAP i=6 type=pps min=3300 max=21000 ma=3000\n"));
    PASS("nonblocking USB adapter transports one real PD snapshot reply");
    m5_session_usb_reset(ss);
    assert(m5_bridge_publish_pd17(ss,pe,rd,true,h));
    fake_io_t failed={"STATUS\n",0,"",0,4};
    io.context=&failed;
    for(unsigned i=0;i<11;i++)m5_usb_io_poll(ss,&io,false);
    assert(failed.tx_len==0&&m5_session_has_reply(ss));
    for(unsigned i=0;i<5;i++)m5_usb_io_poll(ss,&io,false);
    assert(!strcmp(failed.tx,"STATUS state=ready v=1 pps=1 pos=6 mv=7000 ma=1000\n"));
    PASS("USB TX backpressure retries without duplicating the reply");
    m5_session_usb_reset(ss);
    assert(m5_bridge_publish_pd17(ss,pe,rd,true,h));
    fake_io_t reset={"GET_CAPS\n",0,"",0,0};
    io.context=&reset;
    m5_usb_io_poll(ss,&io,false);
    m5_usb_io_poll(ss,&io,true); /* M8F_RecoveryPoll reset event */
    assert(!m5_session_has_reply(ss));
    /* Realtime USB ISR RX ring discard is a separate adapter obligation.
       Mimic caller discarding pre-reset data. */
    reset.cursor=strlen(reset.in);
    m5_usb_io_poll(ss,&io,false);
    assert(reset.tx_len==0);
    PASS("reset event discards a partial pre-reset USB command");
    /* Missing USB readiness/callbacks never block or drop the response. */
    m5_session_usb_reset(ss);
    assert(m5_bridge_publish_pd17(ss,pe,rd,true,h));
    send(ss,"INFO\n");
    io.tx_submit=NULL;
    for(unsigned i=0;i<100;i++)m5_usb_io_poll(ss,&io,false);
    assert(m5_session_has_reply(ss));
    get(ss,"INFO board=CH32X035F8U6 firmware=m5_pd17_bridge protocol=7\n");
    PASS("unavailable EP3 transmit callback does not lose data");
}
static void recovery_test(void){
    m5_usb_recovery_nb_t r;
    m5_usb_recovery_nb_init(&r);
    m5_usb_recovery_nb_submit_begin(&r,100u);
    m5_usb_recovery_nb_submit_result(&r,true);
    assert(m5_usb_recovery_nb_poll(&r,2599u)==M5_USB_EVENT_NONE);
    assert(m5_usb_recovery_nb_poll(&r,2600u)==M5_USB_EVENT_DETACH);
    assert(r.tx_timeouts==1&&r.state==M5_USB_DISCONNECTED_WAIT);
    PASS("nonblocking USB recovery detects 2500ms EP3 deadline");
    for(uint32_t t=2601;t<2750;t++){
        assert(m5_usb_recovery_nb_poll(&r,t)==M5_USB_EVENT_NONE);
    }
    assert(m5_usb_recovery_nb_poll(&r,2750)==M5_USB_EVENT_REINIT);
    assert(r.reconnects==1&&r.state==M5_USB_ACTIVE);
    PASS("150ms detach hold yields to PD loop instead of blocking");
    m5_usb_recovery_nb_submit_begin(&r,UINT32_MAX-100u);
    assert(m5_usb_recovery_nb_poll(&r,2398u)==M5_USB_EVENT_NONE);
    assert(m5_usb_recovery_nb_poll(&r,2399u)==M5_USB_EVENT_DETACH);
    PASS("USB recovery deadline handles 32-bit millisecond wrap");
    assert(m5_usb_recovery_nb_poll(&r,2548u)==M5_USB_EVENT_NONE);
    assert(m5_usb_recovery_nb_poll(&r,2549u)==M5_USB_EVENT_REINIT);
    PASS("USB reconnect hold handles 32-bit time wrap");
    m5_usb_recovery_nb_submit_begin(&r,3000u);
    m5_usb_recovery_nb_submit_result(&r,false);
    assert(m5_usb_recovery_nb_poll(&r,50000u)==M5_USB_EVENT_NONE);
    PASS("rejected endpoint submit cannot start false timeout");
    m5_usb_recovery_nb_submit_begin(&r,3000u);
    m5_usb_recovery_nb_submit_result(&r,true);
    m5_usb_recovery_nb_complete(&r);
    assert(m5_usb_recovery_nb_poll(&r,6000u)==M5_USB_EVENT_NONE);
    PASS("IN completion cancels timeout tracking");
    m5_usb_recovery_nb_submit_begin(&r,7000u);
    m5_usb_recovery_nb_bus_reset(&r);
    assert(m5_usb_recovery_nb_poll(&r,7001u)==M5_USB_EVENT_DROP_PENDING);
    assert(m5_usb_recovery_nb_poll(&r,10000u)==M5_USB_EVENT_NONE);
    PASS("bus reset drops stale work without triggering a delayed timeout");
    assert(r.tx_timeouts==2&&r.reconnects==2&&r.bus_resets==1);
    PASS("health counts match timeout, reconnect, bus-reset events");
}
static void test(void){
    pe_t pe=pps_ready(),before=pe;
    pd_sensor_reading_t rd=reading();
    m5_usb_health_t h={4,0,0};
    m5_session_t ss;
    m5_session_init(&ss);
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    assert(memcmp(&pe,&before,sizeof(pe))==0);
    PASS("PD17 snapshot does not mutate Policy Engine");
    check(&ss,"INFO\n","INFO board=CH32X035F8U6 firmware=m5_pd17_bridge protocol=7\n");
    PASS("INFO v7 identifies new integration prototype");
    check(&ss,"HEALTH\n","HEALTH bus_resets=4 tx_timeouts=0 reconnects=0\n");
    PASS("HEALTH M8F counter schema");
    check(&ss,"GET_CAPS\n","CAPS status=ok count=6 epoch=1\n");
    PASS("GET_CAPS PD17 capability count epoch");
    check(&ss,"GET_CAP 1\n","CAP i=1 type=fixed min=5000 max=5000 ma=3000\n");
    PASS("GET_CAP fixed PDO from real PD17 host fixture");
    check(&ss,"GET_CAP 6\n","CAP i=6 type=pps min=3300 max=21000 ma=3000\n");
    PASS("GET_CAP PPS APDO from real PD17 host fixture");
    check(&ss,"STATUS\n","STATUS state=ready v=1 pps=1 pos=6 mv=7000 ma=1000\n");
    PASS("STATUS requested values not measured values");
    check(&ss,"MEASURE\n","MEASURE status=ok voltage_mV=6965 current_mA=-50\n");
    PASS("MEASURE sign-aware measured uA to mA truncation");
    check(&ss,"SET_PPS 21000 3000\n","ERR code=read_only\n");
    PASS("non-idempotent PPS command refused");
    check(&ss,"OUTPUT_ON\n","ERR code=read_only\n");
    PASS("output toggle refused");
    check(&ss,"GET_CAP 0\n","ERR code=bad_arg\n");
    PASS("zero-index PDO argument rejected");
    check(&ss,"GET_CAP 7\n","ERR code=out_of_range\n");
    PASS("unavailable PDO rejected");
    check(&ss,"GET_CAP X\n","ERR code=bad_arg\n");
    PASS("nonnumeric PDO argument rejected");
    check(&ss,"get_caps\n","ERR code=unknown_command\n");
    PASS("command names are case sensitive");
    check(&ss,"GET_CAPS\r\n","CAPS status=ok count=6 epoch=1\n");
    PASS("CRLF terminal framing accepted");
    check(&ss,"GET_\rCAPS\n","ERR code=bad_line\n");
    PASS("embedded CR rejected");
    check(&ss,"\n","ERR code=bad_line\n");
    PASS("empty line rejected");
    for(unsigned i=0;i<100;i++)assert(m5_session_rx(&ss,'Q'));
    assert(m5_session_rx(&ss,'\n'));
    get(&ss,"ERR code=bad_line\n");
    PASS("overlength line discarded through LF");
    // A max-length request is 63 bytes including LF, even if body is valid.
    for(unsigned i=0;i<63;i++)assert(m5_session_rx(&ss,'Q'));
    assert(m5_session_rx(&ss,'\n'));
    get(&ss,"ERR code=bad_line\n");
    PASS("request LF included in 63-byte bound");
    // Try to insert a new RX command while the existing reply is pending.
    send(&ss,"STATUS\n");
    assert(!m5_session_rx(&ss,'H'));
    get(&ss,"STATUS state=ready v=1 pps=1 pos=6 mv=7000 ma=1000\n");
    check(&ss,"HEALTH\n","HEALTH bus_resets=4 tx_timeouts=0 reconnects=0\n");
    PASS("pending response backpressures RX without overwriting it");
    send(&ss,"INFO\n");
    const char *pkt;size_t n=m5_session_tx_peek(&ss,&pkt);
    assert(n>0);
    // Simulate USBFS_SendEndpoint failure: no commit. The same bytes must be reoffered.
    assert(m5_session_tx_peek(&ss,&pkt)==n);
    m5_session_tx_accepted(&ss,n+1); // invalid commit ignored
    assert(m5_session_tx_peek(&ss,&pkt)==n);
    get(&ss,"INFO board=CH32X035F8U6 firmware=m5_pd17_bridge protocol=7\n");
    PASS("USB TX rejection does not lose the pending reply");
    send(&ss,"GET_CAP 6\n");
    m5_session_usb_reset(&ss);
    assert(!m5_session_has_reply(&ss));
    check(&ss,"GET_CAPS\n","ERR code=snapshot_unavailable\n");
    PASS("M8F bus reset discards response and stale snapshot");
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    check(&ss,"GET_CAP 6\n","CAP i=6 type=pps min=3300 max=21000 ma=3000\n");
    PASS("fresh snapshot after reconnect allows new GET_CAP");
    send(&ss,"GET_CAPS\n");
    pe.state=PE_DETACHED;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,false,&h));
    assert(!m5_session_has_reply(&ss));
    check(&ss,"GET_CAPS\n","CAPS status=unavailable count=0 epoch=1\n");
    PASS("PD detach invalidates unsent capabilities reply");
    check(&ss,"STATUS\n","STATUS state=detached v=0 pps=0 pos=0 mv=0 ma=0\n");
    PASS("detach hides old contract");
    check(&ss,"MEASURE\n","MEASURE status=error reason=sensor_unavailable\n");
    PASS("detach hides old measurement");
    pe=pps_ready();pe.state=PE_RECOVERY;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,false,&h));
    check(&ss,"STATUS\n","STATUS state=recovery v=0 pps=0 pos=0 mv=0 ma=0\n");
    PASS("PD recovery fails closed");
    pe=pps_ready();pe.caps_fresh=false;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,false,&h));
    check(&ss,"GET_CAPS\n","CAPS status=unavailable count=0 epoch=1\n");
    PASS("stale capabilities never advertised");
    pe=pps_ready();pe.state=PE_WAIT_ACCEPT;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,false,&h));
    check(&ss,"STATUS\n","STATUS state=wait_accept v=0 pps=0 pos=0 mv=0 ma=0\n");
    PASS("pending PD negotiation not shown as valid contract");
    pe=pps_ready();pe.caps.count=8;
    assert(!m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    check(&ss,"STATUS\n","ERR code=snapshot_unavailable\n");
    PASS("corrupt cap count suppresses whole snapshot");
    pe=pps_ready();rd.ina_current_ua=INT32_MIN;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    check(&ss,"MEASURE\n","MEASURE status=ok voltage_mV=6965 current_mA=-2147483\n");
    PASS("INT32_MIN signed current truncation without undefined behavior");
    rd.ina_current_ua=-999;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    check(&ss,"MEASURE\n","MEASURE status=ok voltage_mV=6965 current_mA=0\n");
    rd.ina_current_ua=1999;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    check(&ss,"MEASURE\n","MEASURE status=ok voltage_mV=6965 current_mA=1\n");
    PASS("current uA -> mA truncates toward zero as M5-2 reference");
    h.bus_resets=UINT32_MAX;h.tx_timeouts=UINT32_MAX;h.reconnects=UINT32_MAX;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    check(&ss,"HEALTH\n","ERR code=reply_too_long\n");
    PASS("too-long reply fails closed instead of truncating");
    pe=pps_ready();pe.caps.pdo[5]=0x80000000u;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    check(&ss,"GET_CAP 6\n","CAP i=6 type=unsupported raw=0x80000000\n");
    PASS("unsupported PDO preserves raw bits");
    pe=pps_ready();rd=reading();h.bus_resets=4;h.tx_timeouts=0;h.reconnects=0;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,false,&h));
    check(&ss,"MEASURE\n","MEASURE status=error reason=sensor_unavailable\n");
    PASS("aged sensor data cannot return numeric values");
    // Snapshot change while STATUS waiting for USB TX availability.
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    send(&ss,"STATUS\n");
    pe.caps_epoch++;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    assert(!m5_session_has_reply(&ss));
    PASS("capability epoch change cancels unsent STATUS");
    // Keep response intact across sensor-only refresh if PD contract unchanged.
    send(&ss,"STATUS\n");
    rd.ina_bus_mv=7100;
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    get(&ss,"STATUS state=ready v=1 pps=1 pos=6 mv=7000 ma=1000\n");
    PASS("sensor update does not starve pending PD status");
    m5_session_usb_reset(&ss);
    for(const char *q="GET_CA";*q;q++)assert(m5_session_rx(&ss,*q));
    m5_session_usb_reset(&ss);
    assert(m5_bridge_publish_pd17(&ss,&pe,&rd,true,&h));
    check(&ss,"INFO\n","INFO board=CH32X035F8U6 firmware=m5_pd17_bridge protocol=7\n");
    PASS("reset cancels partial pre-disconnect command");
    io_test(&ss,&h,&pe,&rd);
    recovery_test();
    assert(ss.commands_completed>=30);
    printf("PASS: M5 PD17 snapshot -> read-only CDC protocol + M8F recovery boundary (%u/%u)\n",passed,passed);
}
int main(void){test();return 0;}
