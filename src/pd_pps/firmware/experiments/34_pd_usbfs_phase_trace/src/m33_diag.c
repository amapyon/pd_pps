#include "m33_diag.h"
#include "ch32fun.h"
#include "fsusb.h"
#include <string.h>

/* M33: bounded, read-only event history.  IRQ hooks do not allocate, print,
 * divide, read PD peripherals, or operate on USB hardware.  The only shared
 * state changed by the foreground is the capture gate and time approximation.
 * A reset loses the RAM log. */
#define M33_EVENTS 40u
#define M33_TOKEN_LIMIT 8u
#define M33_RESET_LIMIT 8u
#define M33_SETUP_LIMIT 8u
#define M33_CFG_LIMIT 4u
#define M33_MISC_LIMIT 4u

typedef struct {
    uint32_t ms;
    uint16_t flags;
    uint8_t type, ep, cfg, udev;
} event_t;
static event_t events[M33_EVENTS];
static volatile uint8_t used, logging, attached_now, pe_now;
static volatile uint32_t foreground_ms;
static volatile uint32_t all_in,all_out,all_setup,all_reset,all_suspend;
static volatile uint32_t session_in,session_out,session_setup,session_reset,session_suspend;
static uint8_t shown_in,shown_out,shown_setup,shown_reset,shown_cfg,shown_misc;
static uint32_t attach_ms,detach_ms;
static uint32_t attach_in,attach_out,attach_setup,attach_reset;
static uint8_t attach_cfg,detach_cfg,attach_udev,detach_udev;
static uint8_t prev_attached, seen_session;

static void note(char type,uint8_t ep,uint16_t flags,uint8_t cfg,uint8_t udev) {
    if (!logging || used>=M33_EVENTS) return;
    uint8_t *quota=0; uint8_t limit=0;
    switch(type) {
    case 'I':quota=&shown_in;limit=M33_TOKEN_LIMIT;break;
    case 'O':quota=&shown_out;limit=M33_TOKEN_LIMIT;break;
    case 'S':quota=&shown_setup;limit=M33_SETUP_LIMIT;break;
    case 'R':quota=&shown_reset;limit=M33_RESET_LIMIT;break;
    case 'C':quota=&shown_cfg;limit=M33_CFG_LIMIT;break;
    default:quota=&shown_misc;limit=M33_MISC_LIMIT;break;
    }
    if(*quota>=limit)return;
    ++*quota;
    event_t *e=&events[used];
    e->ms=foreground_ms; e->flags=flags; e->type=(uint8_t)type;
    e->ep=ep; e->cfg=cfg; e->udev=udev;
    ++used; /* Publish completed slot last. */
}
void m33_diag_init(uint32_t now) {
    used=logging=attached_now=pe_now=0;
    foreground_ms=now;
    all_in=all_out=all_setup=all_reset=all_suspend=0;
    session_in=session_out=session_setup=session_reset=session_suspend=0;
    shown_in=shown_out=shown_setup=shown_reset=shown_cfg=shown_misc=0;
    attach_ms=detach_ms=0;
    attach_in=attach_out=attach_setup=attach_reset=0;
    attach_cfg=detach_cfg=attach_udev=detach_udev=0;
    prev_attached=seen_session=0;
}
void m33_diag_usb_irq(uint16_t intfgst) {
    /* The existing fsusb ISR handles the events.  Only observe the status. */
    if(intfgst & CRB_UIF_TRANSFER) {
        uint8_t token=(uint8_t)((intfgst & CMASK_UIS_TOKEN)>>12);
        uint8_t ep=(uint8_t)((intfgst & CMASK_UIS_ENDP)>>8);
        if(token==CUIS_TOKEN_IN) {
            ++all_in;if(logging)++session_in;
            if(logging)note('I',ep,intfgst,USBFSCTX.USBFS_DevConfig,USBFS->UDEV_CTRL);
        } else if(token==CUIS_TOKEN_OUT) {
            ++all_out;if(logging)++session_out;
            if(logging)note('O',ep,intfgst,USBFSCTX.USBFS_DevConfig,USBFS->UDEV_CTRL);
        } else if(token==CUIS_TOKEN_SETUP) {
            ++all_setup;if(logging)++session_setup;
            if(logging)note('S',ep,intfgst,USBFSCTX.USBFS_DevConfig,USBFS->UDEV_CTRL);
        }
    } else if(intfgst & CRB_UIF_BUS_RST) {
        ++all_reset;if(logging)++session_reset;
        if(logging)note('R',0,intfgst,USBFSCTX.USBFS_DevConfig,USBFS->UDEV_CTRL);
    } else if(intfgst & CRB_UIF_SUSPEND) {
        ++all_suspend;if(logging)++session_suspend;
        if(logging)note('P',0,intfgst,USBFSCTX.USBFS_DevConfig,USBFS->UDEV_CTRL);
    }
}
void m33_diag_set_config_irq(uint8_t cfg) {
    note('C',0,0,cfg,USBFS->UDEV_CTRL);
}
void m33_diag_tick(uint32_t now,bool attached,uint8_t pe) {
    foreground_ms=now;
    attached_now=attached?1u:0u;
    pe_now=pe;
    if(attached && !prev_attached && !seen_session) {
        /* Snapshot already-established USB enumeration before PD attach. */
        seen_session=1;
        attach_ms=now;
        attach_in=all_in;attach_out=all_out;
        attach_setup=all_setup;attach_reset=all_reset;
        attach_cfg=USBFSCTX.USBFS_DevConfig;
        attach_udev=USBFS->UDEV_CTRL;
        logging=1;
    } else if(!attached && prev_attached && seen_session) {
        logging=0;
        detach_ms=now;
        detach_cfg=USBFSCTX.USBFS_DevConfig;
        detach_udev=USBFS->UDEV_CTRL;
    }
    prev_attached=attached?1u:0u;
}

typedef struct {char *b;unsigned n;} writer_t;
static void chr(writer_t*w,char ch){if(w->n<62u)w->b[w->n++]=ch;}
static void txt(writer_t*w,const char*s){while(*s)chr(w,*s++);}
static void dec(writer_t*w,uint32_t v){char a[10];unsigned n=0;do{a[n++]=(char)('0'+v%10u);v/=10u;}while(v&&n<10u);while(n)chr(w,a[--n]);}
static void hx(writer_t*w,uint32_t v,unsigned d){static const char h[]="0123456789ABCDEF";while(d){--d;chr(w,h[(v>>(d*4u))&15u]);}}
static size_t end(writer_t*w){chr(w,'\n');return w->n;}
size_t m33_diag_reply(const uint8_t*cmd,size_t len,char out[64]) {
    if(!cmd||!out||len<5||memcmp(cmd,"TDIAG",5))return 0;
    writer_t w={out,0};
    if(len==5) {
        txt(&w,"TDIAG n=");dec(&w,used);txt(&w," a=");dec(&w,attached_now);
        txt(&w," seen=");dec(&w,seen_session);txt(&w," pe=");dec(&w,pe_now);
        return end(&w);
    }
    if(len==7 && cmd[5]==' ' && cmd[6]=='C') {
        txt(&w,"TC in=");dec(&w,session_in);txt(&w," out=");dec(&w,session_out);
        txt(&w," set=");dec(&w,session_setup);txt(&w," rst=");dec(&w,session_reset);
        txt(&w," sus=");dec(&w,session_suspend);return end(&w);
    }
    if(len==7 && cmd[5]==' ' && cmd[6]=='P') {
        txt(&w,"TP on=");dec(&w,attach_ms);txt(&w," off=");dec(&w,detach_ms);
        txt(&w," cfg=");hx(&w,attach_cfg,2);chr(&w,'/');hx(&w,detach_cfg,2);
        txt(&w," u=");hx(&w,attach_udev,2);chr(&w,'/');hx(&w,detach_udev,2);
        return end(&w);
    }
    if(len==7 && cmd[5]==' ' && cmd[6]=='B') {
        txt(&w,"TB in=");dec(&w,attach_in);txt(&w," out=");dec(&w,attach_out);
        txt(&w," set=");dec(&w,attach_setup);txt(&w," rst=");dec(&w,attach_reset);
        return end(&w);
    }
    if(len<7||len>8||cmd[5]!=' '){txt(&w,"ERR code=bad_arg");return end(&w);}
    unsigned i=0;
    for(size_t p=6;p<len;p++){
        if(cmd[p]<'0'||cmd[p]>'9'){txt(&w,"ERR code=bad_arg");return end(&w);}
        i=i*10u+(unsigned)(cmd[p]-'0');
    }
    if(i>=used){txt(&w,"ERR code=out_of_range");return end(&w);}
    const event_t *e=&events[i];
    txt(&w,"T i=");dec(&w,i);txt(&w," t=");dec(&w,e->ms);
    txt(&w," ev=");chr(&w,(char)e->type);
    txt(&w," ep=");dec(&w,e->ep);
    txt(&w," fg=");hx(&w,e->flags,4);
    txt(&w," c=");hx(&w,e->cfg,2);
    txt(&w," u=");hx(&w,e->udev,2);
    return end(&w);
}
