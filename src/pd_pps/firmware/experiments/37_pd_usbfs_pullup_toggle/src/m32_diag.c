#include "m32_diag.h"
#include "ch32fun.h"
#include "fsusb.h"
#include <string.h>

/* Captures are foreground only, IRQ hooks are constant-time counter bumps.
 * 12 slots: startup, most recent pre-attach, attach, nine samples, detach.
 * Snapshot data is volatile only in the sense that it is not stored in flash:
 * reset loses the session. Do not perform USB/PD operations in diagnostics. */
#define M32_N 12u
typedef struct {
    uint32_t t, afio, rcc_ahb, rcc_apb1, rcc_apb2, gpio_cfgx;
    uint32_t irq, rst, last_irq_ms;
    uint8_t attached, pe, cfg, base, udev, ien;
} sample_t;
static sample_t samples[M32_N];
static uint8_t n, prev_attached, active;
static uint32_t last_capture_ms;
static volatile uint32_t irq_total, reset_total, last_irq_ms, foreground_now;

/* Read register snapshots only from foreground. */
static void capture(uint32_t ms, uint8_t a, uint8_t pe) {
    if(n >= M32_N) return;
    sample_t *s=&samples[n++];
    s->t=ms; s->attached=a; s->pe=pe;
    s->cfg=USBFSCTX.USBFS_DevConfig;
    s->base=USBFS->BASE_CTRL; s->udev=USBFS->UDEV_CTRL;
    s->ien=USBFS->INT_EN; s->afio=AFIO->CTLR;
    s->rcc_ahb=RCC->AHBPCENR;
    s->rcc_apb1=RCC->APB1PCENR;
    s->rcc_apb2=RCC->APB2PCENR;
    s->gpio_cfgx=GPIOC->CFGXR;
    s->irq=irq_total; s->rst=reset_total; s->last_irq_ms=last_irq_ms;
    last_capture_ms=ms;
}
void m32_diag_init(uint32_t now) {
    n=0; active=0; prev_attached=0;
    irq_total=reset_total=0; foreground_now=last_irq_ms=now;
    capture(now,0,0);
}
void m32_diag_usb_irq(void) { ++irq_total; last_irq_ms=foreground_now; }
void m32_diag_bus_reset_irq(void) { ++reset_total; }
void m32_diag_tick(uint32_t now, bool attached, uint8_t pe) {
    foreground_now=now;
    uint8_t a=attached?1u:0u;
    if (!active && !a) {
        /* Keep slot 1 as last known pre-attach state. */
        if(n<2u) capture(now,0,pe);
        else { n=1; capture(now,0,pe); }
    } else if(a && !prev_attached && !active) {
        active=1;
        /* Record the transition even when it occurs soon after startup. */
        capture(now,1,pe);
    } else if(a && active && n<M32_N-1u &&
              (uint32_t)(now-last_capture_ms)>=250u) {
        capture(now,1,pe);
    } else if(!a && prev_attached && active) {
        /* Always retain detach snapshot in last slot. */
        if(n==M32_N) n=M32_N-1u;
        capture(now,0,pe);
    }
    prev_attached=a;
}
typedef struct {char *p;unsigned len;} wr_t;
static void put(wr_t *w,char c){ if(w->len<62u)w->p[w->len++]=c; }
static void str(wr_t *w,const char *s){while(*s)put(w,*s++);}
static void dec(wr_t *w,uint32_t v) {
    char b[10]; unsigned n=0;
    do {b[n++]=(char)('0'+v%10u);v/=10u;} while(v&&n<10u);
    while(n)put(w,b[--n]);
}
static void hx(wr_t *w,uint32_t v,unsigned digits){
    static const char *h="0123456789ABCDEF";
    while(digits){--digits;put(w,h[(v>>(digits*4u))&15u]);}
}
static size_t finish(wr_t *w){put(w,'\n');return w->len;}
size_t m32_diag_reply(const uint8_t *cmd,size_t len,char out[64]) {
    if(!cmd || !out || len<5u || memcmp(cmd,"XDIAG",5u))return 0;
    wr_t w={out,0};
    if(len==5u) {
        str(&w,"XDIAG n=");dec(&w,n);
        str(&w," irq=");dec(&w,irq_total);
        str(&w," rst=");dec(&w,reset_total);
        str(&w," a=");dec(&w,prev_attached);
        return finish(&w);
    }
    if(len<7u || len>10u || cmd[5]!=' '){str(&w,"ERR code=bad_arg");return finish(&w);}
    unsigned idx=0; size_t i=6;
    if(i>=len||cmd[i]<'0'||cmd[i]>'9'){str(&w,"ERR code=bad_arg");return finish(&w);}
    while(i<len && cmd[i]>='0' && cmd[i]<='9') {idx=idx*10u+(unsigned)(cmd[i]-'0');i++;}
    if(idx>=n){str(&w,"ERR code=out_of_range");return finish(&w);}
    char mode='S';
    if(i<len){
        if(i+2u!=len||cmd[i]!=' '||(cmd[i+1]!='R'&&cmd[i+1]!='I')){
            str(&w,"ERR code=bad_arg");return finish(&w);
        }
        mode=(char)cmd[i+1];
    }
    const sample_t *s=&samples[idx];
    str(&w,"X i=");dec(&w,idx);
    if(mode=='S') {
        str(&w," t=");dec(&w,s->t);
        str(&w," a=");dec(&w,s->attached);
        str(&w," pe=");dec(&w,s->pe);
        str(&w," c=");hx(&w,s->cfg,2);
        str(&w," b=");hx(&w,s->base,2);
        str(&w," u=");hx(&w,s->udev,2);
        str(&w," e=");hx(&w,s->ien,2);
        str(&w," f=");hx(&w,s->afio,8);
    } else if(mode=='R') {
        str(&w," ahb=");hx(&w,s->rcc_ahb,8);
        str(&w," ap1=");hx(&w,s->rcc_apb1,8);
        str(&w," ap2=");hx(&w,s->rcc_apb2,8);
        str(&w," pcx=");hx(&w,s->gpio_cfgx,8);
    } else {
        str(&w," irq=");dec(&w,s->irq);
        str(&w," rst=");dec(&w,s->rst);
        str(&w," last=");dec(&w,s->last_irq_ms);
    }
    return finish(&w);
}
