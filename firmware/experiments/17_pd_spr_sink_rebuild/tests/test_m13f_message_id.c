/* M13f: production PHY MessageID and GoodCRC boundary tests, host only.
 * This mock drives firmware register/ISR entry points, not a physical PD wire. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ch32fun.h"

mock_systick_t mock_systick;
mock_rcc_t mock_rcc;
mock_gpio_t mock_gpioc;
mock_afio_t mock_afio;
mock_usbpd_t mock_usbpd;
static bool irq_enabled;
void NVIC_DisableIRQ(int irq) { (void)irq; irq_enabled=false; }
void NVIC_EnableIRQ(int irq) { (void)irq; irq_enabled=true; }
void Delay_Us(uint32_t us) { (void)us; }
#include "../src/pd_phy_ch32x035.c"

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
    return false; } } while (0)

static void fixture(void) {
    memset(&mock_systick,0,sizeof(mock_systick));
    memset(&mock_rcc,0,sizeof(mock_rcc));
    memset(&mock_gpioc,0,sizeof(mock_gpioc));
    memset(&mock_afio,0,sizeof(mock_afio));
    memset(&mock_usbpd,0,sizeof(mock_usbpd));
    phy_init();
    trace_head=trace_tail=0u; trace_drops=0u;
    USBPD->PORT_CC1=PA_CC_AI;
    phy_poll_cc(1u); phy_poll_cc(22u);
    pe_event_t ev; pe_caps_t c;
    bool got=phy_pop(&ev,&c);
    assert(got && ev == PE_EV_ATTACH && irq_enabled);
}
static uint16_t sent_header(void) {
    uint16_t h=0u;
    memcpy(&h,dma_buffer,sizeof(h));
    return h;
}
static void tx_end(uint32_t ticks) {
    SysTick->CNTL=ticks;
    USBPD->STATUS=IF_TX_END;
    USBPD_IRQHandler();
    USBPD->STATUS=0u;
}
static void ack(uint8_t msg_id,uint32_t ticks) {
    uint16_t h=hdr(PD_CTRL_GOODCRC,0u,msg_id);
    SysTick->CNTL=ticks;
    memcpy(dma_buffer,&h,2u);
    USBPD->BMC_BYTE_CNT=6u;
    USBPD->STATUS=IF_RX_ACT | BMC_AUX_SOP0;
    USBPD_IRQHandler();
    USBPD->STATUS=0u;
}
static bool sixteen_requests_cross_two_mid_wraps(void) {
    fixture();
    for (unsigned i=0; i<17u; ++i) {
        uint8_t id=(uint8_t)(i & 7u);
        uint32_t t=0xffff0000u + i*80000u; /* SysTick wraps during sequence */
        CHECK(phy_send(PE_CMD_REQUEST,0x6302bc14u,i*5u));
        CHECK(((sent_header() >> 9) & 7u)==id);
        CHECK((sent_header() & 0x1fu)==PD_DATA_REQUEST);
        uint32_t body=0u; memcpy(&body,&dma_buffer[2],4u);
        CHECK(body==0x6302bc14u);
        CHECK(tx_wait_crc && tx_expected_id==id);
        tx_end(t);
        CHECK(crc_wait_armed);
        ack(id,t+5000u);
        CHECK(!tx_wait_crc && !crc_wait_armed);
        CHECK(tx_msg_id==(uint8_t)((i+1u)&7u));
        CHECK(retry_count==0u);
    }
    CHECK(!phy_lost_events());
    CHECK(irq_enabled);
    return true;
}
static bool mismatched_goodcrc_at_mid_boundary(void) {
    fixture(); tx_msg_id=7u;
    CHECK(phy_send(PE_CMD_REQUEST,0x6302bc14u,0u));
    tx_end(0xfffff000u);
    ack(0u,0xfffff500u);
    CHECK(tx_wait_crc && tx_msg_id==7u && crc_wait_armed);
    ack(7u,0xfffff600u);
    CHECK(!tx_wait_crc && tx_msg_id==0u);
    CHECK(phy_send(PE_CMD_REQUEST,0x6302bc14u,1u));
    CHECK(((sent_header()>>9)&7u)==0u);
    tx_end(0x00008000u);
    ack(7u,0x00008400u); /* stale acknowledgement of previous send */
    CHECK(tx_wait_crc && tx_msg_id==0u);
    ack(0u,0x00008800u);
    CHECK(!tx_wait_crc && tx_msg_id==1u);
    return true;
}
static bool failure_at_mid7_advances_to_zero(void) {
    fixture(); tx_msg_id=7u;
    CHECK(phy_send(PE_CMD_REQUEST,0x6302bc14u,0u));
    uint32_t t=0xffffb000u;
    for (unsigned retry=0u;retry<3u;++retry) {
        tx_end(t);
        SysTick->CNTL=t+CRC_RECEIVE_TICKS-1u;
        phy_maintain(0u);
        CHECK(tx_wait_crc && retry_count==retry);
        SysTick->CNTL++;
        phy_maintain(0u);
        if (retry<2u) {
            CHECK(tx_wait_crc && retry_count==retry+1u);
            CHECK(tx_msg_id==7u);
        }
        t+=CRC_RECEIVE_TICKS+12000u;
    }
    CHECK(!tx_wait_crc && !crc_wait_armed && tx_msg_id==0u);
    pe_event_t ev; pe_caps_t c;
    CHECK(phy_pop(&ev,&c) && ev==PE_EV_TX_FAILED);
    CHECK(!phy_pop(&ev,&c));
    return true;
}
static const uint32_t source_caps[4]={
    0x0a01912cu,0x0002d0deu,0x0003c0a7u,0xc0dc3224u
};
static void rx_caps(uint8_t mid) {
    uint16_t h=(uint16_t)(PD_DATA_SOURCE_CAP | (1u<<5) | (2u<<6) |
                          (1u<<8) | ((uint16_t)(mid&7u)<<9) | (4u<<12));
    memcpy(dma_buffer,&h,2u);
    memcpy(&dma_buffer[2],source_caps,sizeof(source_caps));
    USBPD->BMC_BYTE_CNT=22u;
    USBPD->STATUS=IF_RX_ACT | BMC_AUX_SOP0;
    USBPD_IRQHandler();
    USBPD->STATUS=0u;
}
static bool incoming_mid7_0_duplicate_filtered(void) {
    fixture();
    pe_event_t ev; pe_caps_t c;
    rx_caps(7u);
    CHECK(phy_pop(&ev,&c) && ev==PE_EV_CAPS && c.msg_id==7u && c.count==4u);
    tx_end(10000u); /* GoodCRC TX end */
    rx_caps(7u); CHECK(!phy_pop(&ev,&c));
    tx_end(11000u);
    rx_caps(0u);
    CHECK(phy_pop(&ev,&c) && ev==PE_EV_CAPS && c.msg_id==0u && c.count==4u);
    tx_end(12000u);
    rx_caps(0u); CHECK(!phy_pop(&ev,&c));
    CHECK(!phy_lost_events());
    return true;
}
static bool hard_reset_invalidates_crc_and_ids(void) {
    fixture(); tx_msg_id=7u;
    CHECK(phy_send(PE_CMD_REQUEST,0x6302bc14u,0u));
    tx_end(10000u);
    CHECK(tx_wait_crc);
    USBPD->STATUS=IF_RX_RESET;
    USBPD_IRQHandler(); USBPD->STATUS=0u;
    CHECK(hard_reset && !tx_wait_crc && !crc_wait_armed && tx_msg_id==0u);
    SysTick->CNTL=10000u+CRC_RECEIVE_TICKS*2u;
    phy_maintain(0u);
    CHECK(!phy_send(PE_CMD_REQUEST,0x6302bc14u,1u));
    pe_event_t ev; pe_caps_t c;
    CHECK(phy_pop(&ev,&c) && ev==PE_EV_HARD_RESET_RX);
    return true;
}

typedef bool (*case_fn)(void);
static struct {const char *name;case_fn fn;} cases[]={
    {"17 TX MessageIDs / 2 wraps / GoodCRC",sixteen_requests_cross_two_mid_wraps},
    {"wrong GoodCRC at 7->0 never advances MID",mismatched_goodcrc_at_mid_boundary},
    {"terminal TX failure consumes MID 7->0",failure_at_mid7_advances_to_zero},
    {"RX 7->0 new / same-id duplicate ignored",incoming_mid7_0_duplicate_filtered},
    {"Hard Reset cancels outstanding CRC/MID",hard_reset_invalidates_crc_and_ids},
};
int main(void) {
    for (unsigned i=0u;i<sizeof(cases)/sizeof(cases[0]);++i) {
        if (!cases[i].fn()) return 1;
        printf("PASS %02u %s\n",i+1u,cases[i].name);
    }
    printf("M13f PHY MessageID: %zu/%zu PASS\n",
           sizeof(cases)/sizeof(cases[0]),sizeof(cases)/sizeof(cases[0]));
    return 0;
}
