/* M13g: policy Reject/unrequested WAIT must stop real PHY TX/GoodCRC
   until a physical CC detach. Production modules + USBPD register mocks. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ch32fun.h"
#include "pd_policy.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return false;}}while(0)
mock_systick_t mock_systick;
mock_rcc_t mock_rcc;
mock_gpio_t mock_gpioc;
mock_afio_t mock_afio;
mock_usbpd_t mock_usbpd;
static bool irq_enabled;
void NVIC_DisableIRQ(int irq){(void)irq;irq_enabled=false;}
void NVIC_EnableIRQ(int irq){(void)irq;irq_enabled=true;}
void Delay_Us(uint32_t us){(void)us;}
#include "../src/pd_phy_ch32x035.c"
static const pe_caps_t caps={.pdo={0x0801912cu,0x0002d12cu,0x0003c12cu,
 0x0004b12cu,0x000641f4u,0xc1a4213cu},.count=6u};
static void setup(pe_t *p) {
 memset(&mock_systick,0,sizeof mock_systick);
 memset(&mock_rcc,0,sizeof mock_rcc);
 memset(&mock_gpioc,0,sizeof mock_gpioc);
 memset(&mock_afio,0,sizeof mock_afio);
 memset(&mock_usbpd,0,sizeof mock_usbpd);
 irq_enabled=false;phy_init();pe_init(p);
 USBPD->PORT_CC1=PA_CC_AI;
 phy_poll_cc(1u);phy_poll_cc(25u);
 pe_event_t ev;pe_caps_t c;
 if(phy_pop(&ev,&c) && ev==PE_EV_ATTACH) pe_event(p,ev,NULL,25u);
}
static bool verify_stopped(void) {
 CHECK(phy_vbus_loss_latched() && !phy_hard_reset_seen());
 CHECK(!phy_send(PE_CMD_REQUEST,0x6302bc14u,5000u));
 uint16_t h=hdr(PD_CTRL_ACCEPT,0u,4u);
 memcpy(dma_buffer,&h,2u);
 USBPD->BMC_BYTE_CNT=6u;USBPD->STATUS=IF_RX_ACT|BMC_AUX_SOP0;
 USBPD_IRQHandler();USBPD->STATUS=0u;
 pe_event_t ev;pe_caps_t c;
 CHECK(!phy_pop(&ev,&c));
 CHECK(!tx_wait_crc && !crc_wait_armed && retry_count==0u);
 return true;
}
static bool reattach_rearms(pe_t *p) {
 USBPD->PORT_CC1=0u;
 phy_poll_cc(100u);phy_poll_cc(260u);
 pe_event_t e;pe_caps_t c;
 CHECK(phy_pop(&e,&c) && e==PE_EV_DETACH);
 pe_event(p,e,NULL,260u);
 phy_reset_after_detach();
 CHECK(!phy_vbus_loss_latched());
 USBPD->PORT_CC1=PA_CC_AI;
 phy_poll_cc(280u);phy_poll_cc(305u);
 CHECK(phy_pop(&e,&c) && e==PE_EV_ATTACH);
 pe_event(p,e,NULL,305u);
 CHECK(p->state==PE_WAIT_CAPS && irq_enabled);
 pe_event(p,PE_EV_CAPS,&caps,310u);
 CHECK(p->command==PE_CMD_REQUEST && phy_send(p->command,p->rdo,310u));
 return true;
}
static bool test_unsent_wait_phy_abort(void){
 pe_t p;setup(&p);
 CHECK(phy_attached() && irq_enabled);
 pe_event(&p,PE_EV_CAPS,&caps,30u);
 CHECK(p.command==PE_CMD_REQUEST && !p.request_sent);
 pe_event(&p,PE_EV_WAIT,NULL,31u);
 CHECK(p.state==PE_RECOVERY && p.command==PE_CMD_NONE);
 CHECK(phy_abort_for_policy_failure(PE_WAIT_ACCEPT));
 CHECK(verify_stopped());CHECK(reattach_rearms(&p));return true;
}
static bool test_pps_reject_phy_abort(void){
 pe_t p;setup(&p);
 pe_event(&p,PE_EV_CAPS,&caps,30u);
 pe_command_result(&p,true,31u);
 pe_event(&p,PE_EV_ACCEPT,NULL,32u);
 pe_event(&p,PE_EV_PS_RDY,NULL,34u);
 CHECK(p.state==PE_READY && p.contract_valid && p.pps_active);
 pe_tick(&p,5034u);
 CHECK(p.command==PE_CMD_REQUEST);
 CHECK(phy_send(p.command,p.rdo,5034u));
 pe_command_result(&p,true,5034u);
 pe_event(&p,PE_EV_REJECT,NULL,5036u);
 CHECK(p.state==PE_RECOVERY && p.command==PE_CMD_NONE);
 CHECK(phy_abort_for_policy_failure(PE_WAIT_ACCEPT));
 CHECK(verify_stopped());CHECK(reattach_rearms(&p));return true;
}
int main(void){
 if(!test_unsent_wait_phy_abort())return 1;
 puts("PASS unsent WAIT stops PHY TX/GoodCRC, CC rearm");
 if(!test_pps_reject_phy_abort())return 1;
 puts("PASS PPS REJECT stops PHY TX/GoodCRC, CC rearm");
 puts("M13g Policy/PHY fail-closed: 2/2 PASS");return 0;
}
