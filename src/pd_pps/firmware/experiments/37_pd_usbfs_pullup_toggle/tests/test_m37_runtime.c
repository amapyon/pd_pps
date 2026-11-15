#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ch32fun.h"
#include "fsusb.h"
#include "m37_pullup.h"
usbfs_regs_t USBFS_REGS;
afio_regs_t AFIO_REGS;
gpio_regs_t GPIOC_REGS;
static int disable_count, setup_count;
void NVIC_DisableIRQ(int irq){assert(irq==USB_IRQn);++disable_count;}
int USBFSSetup(void){
    ++setup_count;
    USBFS_REGS.BASE_CTRL=0x29;
    USBFS_REGS.UDEV_CTRL=0x21;
    USBFS_REGS.INT_EN=7;
    AFIO_REGS.CTLR |= USB_IOEN | USB_PHY_V33 | (3u<<2);
    GPIOC_REGS.CFGXR = (GPIOC_REGS.CFGXR & ~0xffu) | 0x84;
    return 0;
}
static void req(const char *s,const char *prefix) {
    char out[64]={0};
    size_t n=m37_pullup_reply((const uint8_t*)s,strlen(s),out);
    assert(n>0 && n<64 && out[n-1]=='\n');
    assert(strncmp(out,prefix,strlen(prefix))==0);
}
int main(void) {
    m37_pullup_init();
    AFIO_REGS.CTLR=0x000C03CC;
    GPIOC_REGS.CFGXR=0x44444484;
    USBFS_REGS.BASE_CTRL=0x29;
    USBFS_REGS.UDEV_CTRL=0x31;
    USBFS_REGS.INT_EN=7;
    uint32_t before_afio=AFIO_REGS.CTLR, before_gpio=GPIOC_REGS.CFGXR;
    assert(!m37_pullup_usb_service_blocked());
    assert(m37_pullup_led_interval_ms()==250u);
    m37_pullup_tick(1000,1,4);
    m37_pullup_tick(30999,1,4);
    assert(AFIO_REGS.CTLR==before_afio);
    m37_pullup_tick(31000,1,4);
    assert(m37_pullup_usb_service_blocked());
    assert(m37_pullup_led_interval_ms()==100u);
    assert(AFIO_REGS.CTLR == (before_afio & ~UDP_PUE_MASK));
    assert(GPIOC_REGS.CFGXR==before_gpio);
    assert(USBFS_REGS.BASE_CTRL==0x29 && USBFS_REGS.INT_EN==7 && USBFS_REGS.UDEV_CTRL==0x31);
    assert(disable_count==0);
    m37_pullup_tick(60999,1,4);
    assert((AFIO_REGS.CTLR & UDP_PUE_MASK)==0);
    m37_pullup_tick(61000,1,4);
    assert(m37_pullup_led_interval_ms()==500u);
    assert(AFIO_REGS.CTLR==before_afio);
    assert(USBFS_REGS.BASE_CTRL==0x29 && USBFS_REGS.INT_EN==7);
    assert(disable_count==0);
    m37_pullup_tick(90999,1,4);
    assert(USBFS_REGS.BASE_CTRL==0x29);
    m37_pullup_tick(91000,1,4);
    assert(m37_pullup_led_interval_ms()==1000u);
    assert(disable_count==1);
    assert(USBFS_REGS.BASE_CTRL==0 && USBFS_REGS.INT_EN==0);
    assert(USBFS_REGS.UDEV_CTRL==USBFS_UD_PD_DIS);
    assert((AFIO_REGS.CTLR & (USB_IOEN | UDP_PUE_MASK | UDM_PUE_MASK))==0);
    assert((GPIOC_REGS.CFGXR & 0xffu)==0x44);
    m37_pullup_tick(102000,0,0);
    m37_pullup_tick(102499,0,0);
    assert(setup_count==0);
    m37_pullup_tick(102500,0,0);
    assert(setup_count==1 && !m37_pullup_usb_service_blocked());
    req("RUDIAG","RUDIAG n=9 state=6 blocked=0");
    req("RUDIAG S0","VS i=0 ph=A");
    req("RUDIAG S1","VS i=1 ph=B");
    req("RUDIAG S2","VS i=2 ph=U");
    req("RUDIAG S3","VS i=3 ph=C");
    req("RUDIAG S4","VS i=4 ph=N");
    req("RUDIAG S5","VS i=5 ph=F");
    req("RUDIAG S6","VS i=6 ph=O");
    req("RUDIAG S7","VS i=7 ph=D");
    req("RUDIAG S8","VS i=8 ph=R");
    req("RUDIAG R2","VR i=2 f=000C03C0 g=44444484");
    req("RUDIAG R4","VR i=4 f=000C03CC g=44444484");
    req("RUDIAG S99","ERR code=out_of_range");
    req("RUDIAG Q1","ERR code=bad_arg");
    /* Unexpected detach during B and rapid reattach must stay isolated. */
    m37_pullup_init();
    AFIO_REGS.CTLR=0x000C03CC;
    GPIOC_REGS.CFGXR=0x44444484;
    USBFS_REGS.BASE_CTRL=0x29;
    USBFS_REGS.INT_EN=7;
    m37_pullup_tick(2000,1,4);
    m37_pullup_tick(32000,1,4);
    m37_pullup_tick(34000,0,0);
    assert(USBFS_REGS.BASE_CTRL==0 && m37_pullup_usb_service_blocked());
    m37_pullup_tick(34300,1,4);
    assert(USBFS_REGS.BASE_CTRL==0 && m37_pullup_usb_service_blocked());
    m37_pullup_tick(35000,0,0);
    m37_pullup_tick(35500,0,0);
    assert(setup_count==2);
    puts("M37 mocked runtime PASS");
    return 0;
}
