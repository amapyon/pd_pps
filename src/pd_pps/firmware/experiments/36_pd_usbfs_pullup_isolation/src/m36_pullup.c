/* M36: three-stage hardware diagnostic with the PC USB data cable removed.
 * A: 0..30s after PD attach: original USBFS and PC17 D+ pull-up.
 * B: 30..60s: USBFS still enabled, ONLY UDP_PUE[1:0]=00.
 * C: 60s+: USBFS, USB pin mux and pull-ups off; PC16/17 floating.
 * After PD detach +500ms, restore USBFS with USBFSSetup for RAM reading.
 * DO NOT use as a USB CDC recovery algorithm. PD PHY/CC are untouched.
 * PVD-based 3.3V USB PHY selection from M31A remains intact.
 */
#include "m36_pullup.h"
#include "ch32fun.h"
#include "fsusb.h"
#include <string.h>

#define M36_A_MS 30000u
#define M36_B_MS 30000u
#define M36_RESTORE_MS 500u
#define M36_SLOTS 7u

typedef enum {
    ST_WAIT=0, ST_BASELINE=1, ST_NO_PULLUP=2, ST_PHY_OFF=3,
    ST_RESTORE=4, ST_DONE=5
} m36_state_t;
typedef struct {
    uint32_t ms, afio, gpio;
    uint8_t ph, base, udev, ien, attached, pe;
} shot_t;
static shot_t shots[M36_SLOTS];
static uint8_t nshots;
static m36_state_t stage;
static uint32_t attached_at, b_started_at, detached_at;

static void snap(uint32_t now, char ph, bool attached, uint8_t pe) {
    if (nshots >= M36_SLOTS) return;
    shot_t *s=&shots[nshots];
    s->ms=now;
    s->ph=(uint8_t)ph;
    s->attached=(uint8_t)attached;
    s->pe=pe;
    s->afio=AFIO->CTLR;
    s->gpio=GPIOC->CFGXR;
    s->base=USBFS->BASE_CTRL;
    s->udev=USBFS->UDEV_CTRL;
    s->ien=USBFS->INT_EN;
    ++nshots;
}

static void disable_only_dp_pullup(void) {
    /* Per CH32X035 RM V1.9 AFIO_CTLR UDP_PUE[1:0], 00=disabled.
     * USB_IOEN, USB_PHY_V33, USBFS registers, GPIOC CFGXR/BSXR,
     * D- and USBPD settings are deliberately left unchanged.
     * Do not reinitialize USBFS while this stage is running. */
    AFIO->CTLR &= ~((uint32_t)UDP_PUE_MASK);
}

static void isolate_full_usb_phy(void) {
    /* M35-equivalent full isolation; modify PC16/PC17 only.
     * Ensure the PC data cable is physically unplugged. */
    NVIC_DisableIRQ(USB_IRQn);
    USBFS->INT_EN=0;
    USBFS->UDEV_CTRL=USBFS_UD_PD_DIS;
    USBFS->BASE_CTRL=0;
    AFIO->CTLR &= ~((uint32_t)(USB_IOEN | UDP_PUE_MASK | UDM_PUE_MASK));
    GPIOC->CFGXR = (GPIOC->CFGXR & ~0xffu) |
                   ((uint32_t)GPIO_CFGLR_IN_FLOAT << 0) |
                   ((uint32_t)GPIO_CFGLR_IN_FLOAT << 4);
}

void m36_pullup_init(void) {
    memset(shots,0,sizeof shots);
    nshots=0;
    stage=ST_WAIT;
    attached_at=b_started_at=detached_at=0;
}

bool m36_pullup_usb_service_blocked(void) {
    return stage==ST_NO_PULLUP || stage==ST_PHY_OFF || stage==ST_RESTORE;
}
uint32_t m36_pullup_led_interval_ms(void) {
    /* At PB12: A 2Hz, B 5Hz, C 1Hz (toggle intervals, not blink periods). */
    if (stage==ST_NO_PULLUP) return 100u;
    if (stage==ST_PHY_OFF || stage==ST_RESTORE) return 500u;
    return 250u;
}

void m36_pullup_tick(uint32_t now, bool pd_attached, uint8_t pe_state) {
    switch(stage) {
    case ST_WAIT:
        if (pd_attached) {
            attached_at=now;
            snap(now,'A',true,pe_state);
            stage=ST_BASELINE;
        }
        break;
    case ST_BASELINE:
        if (!pd_attached) {
            /* Incomplete run: no PHY modification has occurred. */
            snap(now,'D',false,pe_state);
            stage=ST_DONE;
        } else if ((uint32_t)(now-attached_at)>=M36_A_MS) {
            snap(now,'B',true,pe_state); /* just BEFORE changing pullup */
            disable_only_dp_pullup();
            snap(now,'U',true,pe_state); /* just AFTER changing pullup */
            b_started_at=now;
            stage=ST_NO_PULLUP;
        }
        break;
    case ST_NO_PULLUP:
        if (!pd_attached) {
            detached_at=now;
            snap(now,'D',false,pe_state);
            stage=ST_RESTORE;
        } else if ((uint32_t)(now-b_started_at)>=M36_B_MS) {
            snap(now,'C',true,pe_state); /* pullup-off, PHY still ON */
            isolate_full_usb_phy();
            snap(now,'O',true,pe_state); /* full PHY off */
            stage=ST_PHY_OFF;
        }
        break;
    case ST_PHY_OFF:
        if (!pd_attached) {
            detached_at=now;
            snap(now,'D',false,pe_state);
            stage=ST_RESTORE;
        }
        break;
    case ST_RESTORE:
        if (pd_attached) {
            /* Reattach during restore: ensure PHY is actually isolated,
             * including when detach occurred already in stage B. */
            if (USBFS->BASE_CTRL != 0u) isolate_full_usb_phy();
            stage=ST_PHY_OFF;
        } else if ((uint32_t)(now-detached_at)>=M36_RESTORE_MS) {
            (void)USBFSSetup();
            snap(now,'R',false,pe_state);
            stage=ST_DONE;
        }
        break;
    default: break;
    }
}

typedef struct {char *buf;uint8_t used;} writer_t;
static void put(writer_t*w,char c){if(w->used<62u)w->buf[w->used++]=c;}
static void txt(writer_t*w,const char*s){while(*s)put(w,*s++);}
static void num(writer_t*w,uint32_t v){char d[10];uint8_t n=0;do{d[n++]=(char)('0'+v%10u);v/=10u;}while(v&&n<10u);while(n)put(w,d[--n]);}
static void hex(writer_t*w,uint32_t v,uint8_t width){static const char h[]="0123456789ABCDEF";while(width){--width;put(w,h[(v>>(4u*width))&15u]);}}
static size_t done(writer_t*w){put(w,'\n');return w->used;}
size_t m36_pullup_reply(const uint8_t *cmd,size_t len,char out[64]) {
    if(!cmd||!out||len<6||memcmp(cmd,"PUDIAG",6)) return 0;
    writer_t w={out,0};
    if(len==6) {
        txt(&w,"PUDIAG n=");num(&w,nshots);txt(&w," state=");num(&w,(uint32_t)stage);
        txt(&w," blocked=");num(&w,(uint32_t)m36_pullup_usb_service_blocked());
        return done(&w);
    }
    if(len<9||len>10||cmd[6]!=' '||(cmd[7]!='S'&&cmd[7]!='R')) {
        txt(&w,"ERR code=bad_arg");return done(&w);
    }
    uint32_t idx=0;
    for(size_t p=8;p<len;++p){
        if(cmd[p]<'0'||cmd[p]>'9'){txt(&w,"ERR code=bad_arg");return done(&w);}
        idx=idx*10u+(uint32_t)(cmd[p]-'0');
    }
    if(idx>=nshots){txt(&w,"ERR code=out_of_range");return done(&w);}
    const shot_t *s=&shots[idx];
    if(cmd[7]=='S'){
        txt(&w,"VS i=");num(&w,idx);txt(&w," ph=");put(&w,(char)s->ph);
        txt(&w," t=");num(&w,s->ms);txt(&w," a=");num(&w,s->attached);
        txt(&w," pe=");num(&w,s->pe);txt(&w," b=");hex(&w,s->base,2u);
        txt(&w," u=");hex(&w,s->udev,2u);txt(&w," e=");hex(&w,s->ien,2u);
    }else{
        txt(&w,"VR i=");num(&w,idx);txt(&w," f=");hex(&w,s->afio,8u);
        txt(&w," g=");hex(&w,s->gpio,8u);
    }
    return done(&w);
}
