#include "m30_diag.h"
#include "ch32fun.h"
#include "fsusb.h"
#include <string.h>

/* Retain the first 11 samples from a PD attachment plus the detach sample.
 * Stops sampling when full, to preserve the failure onset rather than only
 * the last samples after the host has disappeared. All state is RAM only. */
#define M30_RECORDS 12u
typedef struct {
    uint32_t ms;
    uint32_t afio;
    uint8_t attached, pe, cfg, base, udev, inten;
} m30_record_t;
static m30_record_t rec[M30_RECORDS];
static uint8_t count, prev_attached, session_started;
static uint32_t last_sample_ms;
static volatile uint32_t usb_irq_count, usb_reset_count;

static void capture(uint32_t now, bool attached, uint8_t pe) {
    if (count >= M30_RECORDS) return;
    m30_record_t *r = &rec[count++];
    r->ms = now;
    r->attached = attached ? 1u : 0u;
    r->pe = pe;
    r->cfg = USBFSCTX.USBFS_DevConfig;
    r->base = USBFS->BASE_CTRL;
    r->udev = USBFS->UDEV_CTRL;
    r->inten = USBFS->INT_EN;
    r->afio = AFIO->CTLR;
    last_sample_ms = now;
}
void m30_diag_init(uint32_t now) {
    count = 0;
    prev_attached = 0;
    session_started = 0;
    last_sample_ms = now;
    usb_irq_count = 0;
    usb_reset_count = 0;
    capture(now, false, 0u);
}
void m30_diag_usb_irq(void) { ++usb_irq_count; }
void m30_diag_bus_reset_irq(void) { ++usb_reset_count; }
void m30_diag_tick(uint32_t now, bool attached, uint8_t pe) {
    uint8_t a = attached ? 1u : 0u;
    if (a && !prev_attached && !session_started) {
        session_started = 1;
        capture(now, true, pe);
    } else if (a && session_started && count < M30_RECORDS-1u &&
               (uint32_t)(now-last_sample_ms) >= 250u) {
        capture(now, true, pe);
    } else if (!a && prev_attached && session_started) {
        /* Reserve the last record for post-detach, even if already full. */
        if (count == M30_RECORDS) count = M30_RECORDS-1u;
        capture(now, false, pe);
    }
    prev_attached = a;
}

typedef struct { char *p; unsigned n; } writer_t;
static void c(writer_t *w, char ch) { if(w->n<62u) w->p[w->n++]=ch; }
static void str(writer_t *w,const char *s) {while(*s)c(w,*s++);}
static void dec(writer_t *w,uint32_t n) {
    char tmp[10];unsigned k=0;
    do {tmp[k++]=(char)('0'+(n%10u));n/=10u;}while(n && k<10u);
    while(k)c(w,tmp[--k]);
}
static void hex(writer_t *w,uint32_t n,unsigned digs) {
    static const char h[]="0123456789ABCDEF";
    while(digs){--digs;c(w,h[(n>>(digs*4u))&15u]);}
}
static size_t done(writer_t *w) { c(w,'\n'); return w->n; }
size_t m30_diag_reply(const uint8_t *cmd,size_t len,char out[64]) {
    if(!cmd || !out || len<4 || memcmp(cmd,"DIAG",4)) return 0;
    writer_t w={out,0};
    if(len==4) {
        str(&w,"DIAG n=");dec(&w,count);
        str(&w," irq=");dec(&w,usb_irq_count);
        str(&w," rst=");dec(&w,usb_reset_count);
        str(&w," attached=");dec(&w,prev_attached);
    } else if(len>=6 && len<=8 && cmd[4]==' ') {
        unsigned idx=0;
        for(size_t i=5;i<len;i++) {
            if(cmd[i]<'0'||cmd[i]>'9') {str(&w,"ERR code=bad_arg");return done(&w);}
            idx=idx*10u+(unsigned)(cmd[i]-'0');
        }
        if(idx>=count){str(&w,"ERR code=out_of_range");return done(&w);}
        const m30_record_t *r = &rec[idx];
        str(&w,"D i=");dec(&w,idx);
        str(&w," t=");dec(&w,r->ms);
        str(&w," a=");dec(&w,r->attached);
        str(&w," pe=");dec(&w,r->pe);
        str(&w," c=");hex(&w,r->cfg,2u);
        str(&w," b=");hex(&w,r->base,2u);
        str(&w," u=");hex(&w,r->udev,2u);
        str(&w," e=");hex(&w,r->inten,2u);
        str(&w," f=");hex(&w,r->afio,8u);
    } else str(&w,"ERR code=bad_arg");
    return done(&w);
}
