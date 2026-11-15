#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ch32fun.h"
#include "fsusb.h"
#include "m35_phy.h"
usbfs_regs_t USBFS_REGS;
afio_regs_t AFIO_REGS;
gpio_regs_t GPIOC_REGS;
static int disable_count, setup_count;
void NVIC_DisableIRQ(int irq){ assert(irq==USB_IRQn); ++disable_count; }
int USBFSSetup(void){
    ++setup_count;
    USBFS_REGS.BASE_CTRL=0x29;
    USBFS_REGS.UDEV_CTRL=1;
    USBFS_REGS.INT_EN=7;
    AFIO_REGS.CTLR |= USB_IOEN | (3u<<2);
    GPIOC_REGS.CFGXR = (GPIOC_REGS.CFGXR & ~0xffu) | 0x84;
    return 0;
}
static void request(const char *s, const char *prefix) {
    char buf[64]={0};
    size_t n=m35_phy_reply((const uint8_t*)s,strlen(s),buf);
    assert(n>0 && n<64 && buf[n-1]=='\n');
    assert(strncmp(buf,prefix,strlen(prefix))==0);
}
int main(void){
    AFIO_REGS.CTLR=0x000C03CC;
    GPIOC_REGS.CFGXR=0x44444484;
    USBFS_REGS.BASE_CTRL=0x29;
    USBFS_REGS.UDEV_CTRL=0x31;
    USBFS_REGS.INT_EN=7;
    m35_phy_init();
    assert(!m35_phy_is_off());
    m35_phy_tick(1000,1,4);
    m35_phy_tick(30999,1,4);
    assert(!m35_phy_is_off());
    assert(disable_count==0);
    m35_phy_tick(31000,1,4);
    assert(m35_phy_is_off());
    assert(disable_count==1);
    assert(USBFS_REGS.INT_EN==0 && USBFS_REGS.BASE_CTRL==0 && USBFS_REGS.UDEV_CTRL==USBFS_UD_PD_DIS);
    assert((AFIO_REGS.CTLR & (USB_IOEN|UDP_PUE_MASK|UDM_PUE_MASK)) == 0);
    assert((GPIOC_REGS.CFGXR&0xff)==0x44);
    assert((GPIOC_REGS.CFGXR&~0xffu)==(0x44444484 & ~0xffu));
    m35_phy_tick(34000,1,4);
    assert(m35_phy_is_off());
    assert(setup_count==0);
    m35_phy_tick(35000,0,0);
    assert(m35_phy_is_off());
    m35_phy_tick(35499,0,0);
    assert(setup_count==0);
    m35_phy_tick(35500,0,0);
    assert(setup_count==1 && !m35_phy_is_off());
    request("PHYDIAG","PHYDIAG n=5 state=4 off=0");
    request("PHYDIAG S0","PS i=0 ph=A");
    request("PHYDIAG S1","PS i=1 ph=B");
    request("PHYDIAG S2","PS i=2 ph=O");
    request("PHYDIAG S3","PS i=3 ph=D");
    request("PHYDIAG S4","PS i=4 ph=R");
    request("PHYDIAG R2","PR i=2 f=");
    request("PHYDIAG S99","ERR code=out_of_range");
    puts("M35 mocked runtime PASS");
    return 0;
}
