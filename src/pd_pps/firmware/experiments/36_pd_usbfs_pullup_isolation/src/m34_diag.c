#include "m34_diag.h"
#include "ch32fun.h"
#include "fsusb.h"
#include <string.h>
/* Phase-separated snapshots: counters updated in USBFS ISR, captured only in
 * foreground. No USB/PD register writes, logging, or time conversion in ISR.
 * Times are main-loop approximations, not interrupt timestamps. */
#define M34_SLOTS 8u
/* phase codes: B=pre-PD boot, A=attach, 1/3/5=seconds after attach,
 * D=detach detected, H=500ms after detach, J=1500ms after detach. */
typedef struct {
    uint32_t ms, in, out, setup, reset, suspend;
    uint8_t phase, attached, pe, cfg, udev;
} snap_t;
static snap_t snaps[M34_SLOTS];
static volatile uint32_t cnt_in,cnt_out,cnt_setup,cnt_reset,cnt_suspend;
static uint32_t attach_ms, detach_ms;
static uint8_t count, ever_attached, prev_attached, next_attached, next_detached;
static void snapshot(uint32_t now, char phase, bool attached, uint8_t pe) {
    if(count>=M34_SLOTS)return;
    snap_t *s=&snaps[count];
    s->ms=now; s->phase=(uint8_t)phase; s->attached=(uint8_t)attached;
    s->pe=pe; s->cfg=USBFSCTX.USBFS_DevConfig; s->udev=USBFS->UDEV_CTRL;
    s->in=cnt_in; s->out=cnt_out; s->setup=cnt_setup;
    s->reset=cnt_reset; s->suspend=cnt_suspend;
    ++count; /* publish after fields are written */
}
void m34_diag_init(uint32_t now) {
    cnt_in=cnt_out=cnt_setup=cnt_reset=cnt_suspend=0;
    count=ever_attached=prev_attached=next_attached=next_detached=0;
    attach_ms=detach_ms=0;
    snapshot(now,'B',false,0);
}
void m34_diag_usb_irq(uint16_t fg) {
    /* Independent tests: multiple asserted flags are all counted. */
    if(fg & CRB_UIF_TRANSFER) {
        uint8_t token=(uint8_t)((fg & CMASK_UIS_TOKEN)>>12);
        if(token==CUIS_TOKEN_IN)++cnt_in;
        else if(token==CUIS_TOKEN_OUT)++cnt_out;
        else if(token==CUIS_TOKEN_SETUP)++cnt_setup;
    }
    if(fg & CRB_UIF_BUS_RST)++cnt_reset;
    if(fg & CRB_UIF_SUSPEND)++cnt_suspend;
}
void m34_diag_tick(uint32_t now, bool attached, uint8_t pe) {
    if(attached && !prev_attached && !ever_attached) {
        ever_attached=1; attach_ms=now;
        snapshot(now,'A',true,pe);
    }
    if(attached && ever_attached && !next_attached) {
        /* Three fixed checkpoints measured from first PD attachment. */
        static const uint32_t offsets[]={1000u,3000u,5000u};
        if((uint32_t)(now-attach_ms)>=offsets[0]) {
            snapshot(now,'1',true,pe);next_attached=1;
        }
    }
    if(attached && next_attached==1 && (uint32_t)(now-attach_ms)>=3000u){
        snapshot(now,'3',true,pe);next_attached=2;
    }
    if(attached && next_attached==2 && (uint32_t)(now-attach_ms)>=5000u){
        snapshot(now,'5',true,pe);next_attached=3;
    }
    if(!attached && prev_attached && ever_attached) {
        detach_ms=now;
        snapshot(now,'D',false,pe);
        next_detached=1;
    }
    if(!attached && next_detached==1 && (uint32_t)(now-detach_ms)>=500u){
        snapshot(now,'H',false,pe);next_detached=2;
    }
    if(!attached && next_detached==2 && (uint32_t)(now-detach_ms)>=1500u){
        snapshot(now,'J',false,pe);next_detached=3;
    }
    prev_attached=(uint8_t)attached;
}
typedef struct {char *b;uint8_t n;} wr_t;
static void ch(wr_t*w,char c){if(w->n<62u)w->b[w->n++]=c;}
static void tx(wr_t*w,const char*s){while(*s)ch(w,*s++);}
static void dec(wr_t*w,uint32_t v){char d[10];uint8_t n=0;do{d[n++]=(char)('0'+v%10u);v/=10u;}while(v&&n<10u);while(n)ch(w,d[--n]);}
static void hex(wr_t*w,uint32_t v){const char *h="0123456789ABCDEF";ch(w,h[(v>>4)&15]);ch(w,h[v&15]);}
static size_t finish(wr_t*w){ch(w,'\n');return w->n;}
size_t m34_diag_reply(const uint8_t*cmd,size_t len,char out[64]){
    if(!cmd||!out||len<5||memcmp(cmd,"WDIAG",5))return 0;
    wr_t w={out,0};
    if(len==5){tx(&w,"WDIAG n=");dec(&w,count);tx(&w," seen=");dec(&w,ever_attached);return finish(&w);}
    if(len<8 || len>9 || cmd[5]!=' ' || (cmd[6]!='S'&&cmd[6]!='C')){
        tx(&w,"ERR code=bad_arg");return finish(&w);
    }
    uint32_t i=0;
    for(size_t n=7;n<len;++n){
        if(cmd[n]<'0'||cmd[n]>'9'){tx(&w,"ERR code=bad_arg");return finish(&w);}
        i=i*10u+(uint32_t)(cmd[n]-'0');
    }
    if(i>=count){tx(&w,"ERR code=out_of_range");return finish(&w);}
    const snap_t*s=&snaps[i];
    if(cmd[6]=='S'){
        tx(&w,"WS i=");dec(&w,i);tx(&w," ph=");ch(&w,(char)s->phase);
        tx(&w," t=");dec(&w,s->ms);tx(&w," a=");dec(&w,s->attached);
        tx(&w," pe=");dec(&w,s->pe);tx(&w," c=");hex(&w,s->cfg);
        tx(&w," u=");hex(&w,s->udev);
    }else{
        tx(&w,"WC i=");dec(&w,i);tx(&w," in=");dec(&w,s->in);
        tx(&w," out=");dec(&w,s->out);tx(&w," set=");dec(&w,s->setup);
        tx(&w," rst=");dec(&w,s->reset);tx(&w," sus=");dec(&w,s->suspend);
    }
    return finish(&w);
}
