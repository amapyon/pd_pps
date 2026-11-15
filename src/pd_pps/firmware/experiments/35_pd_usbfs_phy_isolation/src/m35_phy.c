/* M35: hardware isolation diagnostic, NOT a USB recovery implementation.
 * Only touch USBFS, AFIO USB I/O/pullup bits and GPIO PC16/17.
 * Do not write USBPD, CC1/CC2 GPIO config, RCC or Policy Engine state.
 * RUN WITH PC USB DATA CABLE DISCONNECTED. Periodic forced detach makes
 * USB unsuitable for normal use during this test.
 *
 * PD CC attached 0..30s  = normal USB PHY (slow LED, 250ms toggle)
 * PD CC attached 30s+    = isolated PHY      (fast LED, 100ms toggle)
 * PD CC detached +500ms  = USBFSSetup, CDC restored; read PHYDIAG over CDC
 * Snapshot memory survives only while VDD33 is maintained.
 */
#include "m35_phy.h"
#include "ch32fun.h"
#include "fsusb.h"
#include <string.h>

#define M35_BASELINE_MS 30000u
#define M35_RESTORE_MS 500u
#define M35_SLOTS 6u

typedef enum {ST_WAIT=0, ST_BASELINE=1, ST_OFF=2, ST_RESTORE=3, ST_DONE=4} m35_state_t;
typedef struct {
    uint32_t ms, afio, gpio;
    uint8_t ph, base, udev, ien, attached, pe;
} shot_t;
static shot_t shots[M35_SLOTS];
static uint8_t nshots;
static m35_state_t stage;
static uint32_t attach_ms, detach_ms;

static void snap(uint32_t now, char ph, bool attached, uint8_t pe) {
    if(nshots>=M35_SLOTS) return;
    shot_t *s=&shots[nshots];
    s->ms=now; s->ph=(uint8_t)ph; s->attached=(uint8_t)attached;
    s->pe=pe; s->afio=AFIO->CTLR; s->gpio=GPIOC->CFGXR;
    s->base=USBFS->BASE_CTRL; s->udev=USBFS->UDEV_CTRL;
    s->ien=USBFS->INT_EN;
    ++nshots;
}

static void disable_usb_phy(void) {
    /* Disable USB IRQ first, then USBFS transceiver and D+ pullup.
     * Do not touch GPIOC lower pin configuration (PC14/CC1, PC15/CC2).
     * CFGXR nibbles 0/1 address ONLY PC16/PC17. */
    NVIC_DisableIRQ(USB_IRQn);
    USBFS->INT_EN=0;
    /* Keep the internal D+/D- pulldown disabled for a high-Z comparison. */
    USBFS->UDEV_CTRL=USBFS_UD_PD_DIS;
    USBFS->BASE_CTRL=0;
    /* Bits: USB_IOEN, UDP_PUE[1:0], UDM_PUE[1:0].
     * Keep USBPD_PHY_V33 and USBPD_IN_HVT untouched. */
    AFIO->CTLR &= ~(USB_IOEN | UDP_PUE_MASK | UDM_PUE_MASK);
    /* Both pins are high-impedance floating inputs instead of USB mode.
     * PC16 is nibble 0, PC17 nibble 1 in GPIOC->CFGXR. */
    GPIOC->CFGXR = (GPIOC->CFGXR & ~0xffu) |
                   ((uint32_t)GPIO_CFGLR_IN_FLOAT << 0) |
                   ((uint32_t)GPIO_CFGLR_IN_FLOAT << 4);
}

void m35_phy_init(void) {
    memset(shots,0,sizeof shots);
    nshots=0;
    stage=ST_WAIT;
    attach_ms=detach_ms=0;
}

bool m35_phy_is_off(void) {
    return stage==ST_OFF || stage==ST_RESTORE;
}

void m35_phy_tick(uint32_t now, bool pd_attached, uint8_t pe_state) {
    switch(stage) {
    case ST_WAIT:
        if(pd_attached) {
            attach_ms=now;
            snap(now,'A',true,pe_state);
            stage=ST_BASELINE;
        }
        break;
    case ST_BASELINE:
        if(!pd_attached) {
            /* Attachment too short for comparison: permit a new attempt. */
            stage=ST_WAIT;
        } else if((uint32_t)(now-attach_ms)>=M35_BASELINE_MS) {
            snap(now,'B',true,pe_state);
            disable_usb_phy();
            snap(now,'O',true,pe_state);
            stage=ST_OFF;
        }
        break;
    case ST_OFF:
        if(!pd_attached) {
            detach_ms=now;
            snap(now,'D',false,pe_state);
            stage=ST_RESTORE;
        }
        break;
    case ST_RESTORE:
        if(pd_attached) {
            /* PD was reattached during delay. Do not initialize USB yet. */
            stage=ST_OFF;
        } else if((uint32_t)(now-detach_ms)>=M35_RESTORE_MS) {
            /* USB reinitialization only after PD has physically detached. */
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
size_t m35_phy_reply(const uint8_t *cmd,size_t len,char out[64]) {
    if(!cmd||!out||len<7||memcmp(cmd,"PHYDIAG",7)) return 0;
    writer_t w={out,0};
    if(len==7) {
        txt(&w,"PHYDIAG n=");num(&w,nshots);txt(&w," state=");num(&w,(uint32_t)stage);
        txt(&w," off=");num(&w,(uint32_t)m35_phy_is_off());
        return done(&w);
    }
    if(len<10||len>11||cmd[7]!=' '||(cmd[8]!='S'&&cmd[8]!='R')) {
        txt(&w,"ERR code=bad_arg");return done(&w);
    }
    uint32_t idx=0;
    for(size_t p=9;p<len;++p){
        if(cmd[p]<'0'||cmd[p]>'9'){txt(&w,"ERR code=bad_arg");return done(&w);}
        idx=idx*10u+(uint32_t)(cmd[p]-'0');
    }
    if(idx>=nshots){txt(&w,"ERR code=out_of_range");return done(&w);}
    const shot_t *s=&shots[idx];
    if(cmd[8]=='S'){
        txt(&w,"PS i=");num(&w,idx);txt(&w," ph=");put(&w,(char)s->ph);
        txt(&w," t=");num(&w,s->ms);txt(&w," a=");num(&w,s->attached);
        txt(&w," pe=");num(&w,s->pe);txt(&w," b=");hex(&w,s->base,2u);
        txt(&w," u=");hex(&w,s->udev,2u);txt(&w," e=");hex(&w,s->ien,2u);
    }else{
        txt(&w,"PR i=");num(&w,idx);txt(&w," f=");hex(&w,s->afio,8u);
        txt(&w," g=");hex(&w,s->gpio,8u);
    }
    return done(&w);
}
