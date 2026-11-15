#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "../src/m28_runtime.h"

static unsigned passed;
#define PASS(label) do { passed++; printf("%02u PASS %s\n",passed,label); } while(0)

static pe_t ready(void) {
    pe_t p = {0};
    p.state = PE_READY; p.contract_valid=true; p.pps_active=true;
    p.caps_fresh=true; p.caps_epoch=1; p.caps.count=6;
    p.caps.pdo[0]=0x0801912cu; p.caps.pdo[5]=0xc1a4213cu;
    p.pdo_index=6; p.requested_mv=7000; p.requested_ma=1000;
    return p;
}
static pd_sensor_reading_t sensor(void) {
    pd_sensor_reading_t r={0}; r.ina_ok=true;r.ina_bus_mv=6965;r.ina_current_ua=-50500;
    return r;
}
typedef struct {
    m28_runtime_t *r;
    char response[200]; size_t response_len;
    unsigned rejects;
    uint32_t now;
} fake_io_t;
static bool rx(void *ctx,uint8_t *b) {
    fake_io_t *f=ctx;
    return m28_transport_pop(&f->r->transport,b);
}
static int tx(void *ctx,const uint8_t *b,size_t n) {
    fake_io_t *f=ctx;
    if(f->rejects){f->rejects--;return -1;}
    m5_usb_recovery_nb_submit_begin(&f->r->recovery,f->now);
    assert(n<=63 && f->response_len+n<sizeof(f->response));
    memcpy(f->response+f->response_len,b,n);
    f->response_len+=n;f->response[f->response_len]=0;
    m5_usb_recovery_nb_submit_result(&f->r->recovery,true);
    return 0;
}
static m28_action_t tick(m28_runtime_t *r, uint32_t now,pe_t *pe,
    pd_sensor_reading_t *rd,bool fresh,fake_io_t *f,bool conf) {
    f->now=now;
    const m5_usb_io_t io={rx,tx,f};
    return m28_runtime_poll(r,now,pe,rd,fresh,&io,conf);
}
static void input(m28_runtime_t *r,const char *s) {
    m28_transport_rx_irq(&r->transport,(const uint8_t*)s,strlen(s));
}
static void run(m28_runtime_t *r,uint32_t *now,pe_t *pe,
    pd_sensor_reading_t *rd,bool fresh,fake_io_t *f,size_t times) {
    while(times--){(void)tick(r,(*now)++,pe,rd,fresh,f,true);}
}
static void tests(void) {
    m28_runtime_t rt;
    m28_runtime_init(&rt);
    pe_t p=ready();pd_sensor_reading_t rd=sensor();
    pe_t before=p;
    fake_io_t f={&rt,"",0,0,0}; uint32_t now=1u;
    PASS("initializer accepts independent USB runtime");
    input(&rt,"INFO\n");run(&rt,&now,&p,&rd,true,&f,20);
    assert(!strcmp(f.response,"INFO board=CH32X035F8U6 firmware=m28_pd_usb_cdc protocol=7\n"));
    assert(!memcmp(&p,&before,sizeof(p)));
    PASS("INFO real integrated firmware identity; PE remains immutable");
    m28_transport_in_complete_irq(&rt.transport);
    run(&rt,&now,&p,&rd,true,&f,1);
    assert(!rt.recovery.in_flight);
    PASS("EP3 IN IRQ completion clears watchdog");
    f.response_len=0;f.response[0]=0;
    input(&rt,"GET_CAP 6\n");run(&rt,&now,&p,&rd,true,&f,25);
    assert(!strcmp(f.response,"CAP i=6 type=pps min=3300 max=21000 ma=3000\n"));
    PASS("live PE PDO/APDO read through IRQ-fed ring and USB submit");
    m28_transport_in_complete_irq(&rt.transport);
    (void)tick(&rt,now++,&p,&rd,true,&f,true);
    f.response_len=0;f.response[0]=0;
    input(&rt,"STATUS\n");run(&rt,&now,&p,&rd,true,&f,25);
    assert(!strcmp(f.response,"STATUS state=ready v=1 pps=1 pos=6 mv=7000 ma=1000\n"));
    PASS("contract request separated from measured current");
    m28_transport_in_complete_irq(&rt.transport);
    (void)tick(&rt,now++,&p,&rd,true,&f,true);
    f.response_len=0;f.response[0]=0;
    input(&rt,"MEASURE\n");run(&rt,&now,&p,&rd,true,&f,25);
    assert(!strcmp(f.response,"MEASURE status=ok voltage_mV=6965 current_mA=-50\n"));
    PASS("sensor valid snapshot uses signed microamps");
    m28_transport_in_complete_irq(&rt.transport);
    (void)tick(&rt,now++,&p,&rd,true,&f,true);
    f.response_len=0;f.response[0]=0;
    input(&rt,"MEASURE\n");run(&rt,&now,&p,&rd,false,&f,25);
    assert(!strcmp(f.response,"MEASURE status=error reason=sensor_unavailable\n"));
    PASS("stale sensor is not published as valid measurement");
    m28_transport_in_complete_irq(&rt.transport);
    (void)tick(&rt,now++,&p,&rd,true,&f,true);
    f.response_len=0;f.response[0]=0;
    input(&rt,"SET_PPS 9000 3000\n");run(&rt,&now,&p,&rd,true,&f,28);
    assert(!strcmp(f.response,"ERR code=read_only\n"));
    PASS("PPS output mutation command rejected");
    m28_transport_in_complete_irq(&rt.transport);
    (void)tick(&rt,now++,&p,&rd,true,&f,true);
    f.response_len=0;f.response[0]=0;
    input(&rt,"GET_CAP 6\n");run(&rt,&now,&p,&rd,true,&f,10);
    /* At this point host has received reply, but no endpoint completion. */
    m28_transport_bus_reset_irq(&rt.transport);
    assert(tick(&rt,now++,&p,&rd,true,&f,true)==M28_NO_ACTION);
    assert(rt.recovery.bus_resets==1u);
    PASS("bus RESET invalidates command/response state without PD command");
    m28_transport_in_complete_irq(&rt.transport);
    (void)tick(&rt,now++,&p,&rd,true,&f,true);
    f.response_len=0;f.response[0]=0;
    input(&rt,"GET_CAPS\n");run(&rt,&now,&p,&rd,true,&f,20);
    assert(strstr(f.response,"CAPS status=ok count=6 epoch=1\n"));
    PASS("fresh post-reset request reuses live snapshot without stale queue");
    m28_transport_in_complete_irq(&rt.transport);
    (void)tick(&rt,now++,&p,&rd,true,&f,true);
    f.response_len=0;f.response[0]=0;
    input(&rt,"STATUS\n");run(&rt,&now,&p,&rd,true,&f,7);
    /* Ready reply may be queued for TX; detach must invalidate it. */
    p.state=PE_DETACHED;p.contract_valid=false;p.caps_fresh=false;
    f.response_len=0;f.response[0]=0;
    run(&rt,&now,&p,&rd,true,&f,10);
    assert(!strstr(f.response,"state=ready"));
    PASS("PD detach prevents publishing stale READY state");
    /* Reset whole runtime for independent timeout test. */
    m28_runtime_init(&rt);p=ready();f=(fake_io_t){&rt,"",0,0,0};now=1000;
    input(&rt,"INFO\n");run(&rt,&now,&p,&rd,true,&f,20);
    assert(rt.recovery.in_flight);
    const uint32_t submit=rt.recovery.submit_ms;
    assert(tick(&rt,submit+2499u,&p,&rd,true,&f,true)==M28_NO_ACTION);
    assert(tick(&rt,submit+2500u,&p,&rd,true,&f,true)==M28_STOP_USB);
    assert(rt.recovery.tx_timeouts==1);
    PASS("EP3 2500ms timeout starts nonblocking physical detach");
    assert(tick(&rt,submit+2649u,&p,&rd,true,&f,false)==M28_NO_ACTION);
    assert(tick(&rt,submit+2650u,&p,&rd,true,&f,false)==M28_START_USB);
    assert(rt.recovery.reconnects==1 && p.state==PE_READY);
    PASS("USB reconnect after 150ms does not mutate PD contract");
    m28_runtime_init(&rt);p=ready();f=(fake_io_t){&rt,"",0,0,0};now=10;
    input(&rt,"MEASURE\n");run(&rt,&now,&p,&rd,true,&f,20);
    assert(rt.recovery.in_flight);
    m28_transport_bus_reset_irq(&rt.transport);
    (void)tick(&rt,now++,&p,&rd,true,&f,true);
    assert(!rt.recovery.in_flight);
    assert(tick(&rt,5000,&p,&rd,true,&f,true)==M28_NO_ACTION);
    PASS("bus RESET cancels pending EP3 timeout");
    m28_runtime_init(&rt);p=ready();f=(fake_io_t){&rt,"",0,0,0};now=1;
    char garbage[300];memset(garbage,'X',sizeof(garbage));
    m28_transport_rx_irq(&rt.transport,(const uint8_t*)garbage,sizeof(garbage));
    assert(rt.transport.overflow && rt.transport.overflow_count==1);
    (void)tick(&rt,now++,&p,&rd,true,&f,true);
    assert(!rt.transport.overflow && rt.transport.head==rt.transport.tail);
    assert(rt.discarded_overflows==1);
    PASS("RX ring overflow fails closed and drops partial commands");
    input(&rt,"PING\n");run(&rt,&now,&p,&rd,true,&f,15);
    assert(!strcmp(f.response,"ERR code=unknown_command\n"));
    PASS("recovery continues processing fresh lines after overflow");
    m28_runtime_init(&rt);p=ready();f=(fake_io_t){&rt,"",0,0,0};now=1;
    f.rejects=3;
    input(&rt,"INFO\n");run(&rt,&now,&p,&rd,true,&f,20);
    assert(f.rejects==0 && !strcmp(f.response,"INFO board=CH32X035F8U6 firmware=m28_pd_usb_cdc protocol=7\n"));
    PASS("USB backpressure retries without duplicate response");
    m28_runtime_init(&rt);p=ready();f=(fake_io_t){&rt,"",0,0,0};now=1;
    input(&rt,"INFO\n");run(&rt,&now,&p,&rd,true,&f,5);
    assert(f.response_len==0);
    (void)tick(&rt,now++,&p,&rd,true,&f,false);
    assert(f.response_len==0);
    run(&rt,&now,&p,&rd,true,&f,3);
    assert(f.response_len>0);
    PASS("USB not configured defers EP3 without losing queued response");
    m28_runtime_init(&rt);p=ready();f=(fake_io_t){&rt,"",0,0,0};
    m5_usb_recovery_nb_submit_begin(&rt.recovery,UINT32_MAX-100u);
    assert(tick(&rt,2398,&p,&rd,true,&f,true)==M28_NO_ACTION);
    assert(tick(&rt,2399,&p,&rd,true,&f,true)==M28_STOP_USB);
    assert(tick(&rt,2548,&p,&rd,true,&f,false)==M28_NO_ACTION);
    assert(tick(&rt,2549,&p,&rd,true,&f,false)==M28_START_USB);
    PASS("EP3 timeout and hold survive 32-bit tick wrap");
    printf("TOTAL M28 host assertions %u\n",passed);
}
int main(void) {tests();return 0;}
