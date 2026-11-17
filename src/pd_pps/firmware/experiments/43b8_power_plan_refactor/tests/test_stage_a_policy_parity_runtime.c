/* Parity against the pinned M43B-5 policy fixture; no MCU dependency. */
#include "pd_policy.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void baseline_pe_init(pe_t *p);
void baseline_pe_event(pe_t *p, pe_event_t e, const pe_caps_t *c, uint32_t now);
void baseline_pe_tick(pe_t *p, uint32_t now);
void baseline_pe_command_result(pe_t *p, bool accepted, uint32_t now);
bool baseline_pe_request_soft_reset(pe_t *p, uint32_t now);
bool baseline_pe_request_source_caps(pe_t *p);
static uint32_t rng;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static void compare(const pe_t *a, const pe_t *b, unsigned seed, unsigned step) {
    if (memcmp(a,b,sizeof(*a))) {
        fprintf(stderr,"POLICY_PARITY_FAIL seed=%u step=%u state=%u/%u faults=%u/%u\n",
                seed,step,(unsigned)a->state,(unsigned)b->state,
                (unsigned)a->faults,(unsigned)b->faults);
        exit(1);
    }
}
static pe_caps_t caps_for(unsigned type) {
    pe_caps_t c={0};
    c.count=2u;
    c.msg_id=(uint8_t)(type%8u);
    c.pdo[0]=(100u<<10)|300u;
    c.pdo[1]=(3u<<30)|(110u<<17)|(33u<<8)|60u;
    switch(type%8u) {
      case 0:break;
      case 1:c.count=1u;break;
      case 2:c.pdo[0]=(180u<<10)|300u;break;
      case 3:c.pdo[1]=(3u<<30)|(60u<<17)|(33u<<8)|60u;break;
      case 4:c.count=0u;break;
      case 5:c.count=8u;break;
      case 6:c.pdo[1]=0u;break;
      case 7:c.pdo[0]=0u;break;
    }
    return c;
}
static void step_pair(pe_t *a, pe_t *b, unsigned which, uint32_t now,
                      const pe_caps_t *caps, unsigned seed, unsigned step) {
    bool x,y;
    if (which <= (unsigned)PE_EV_SENSOR_FAULT) {
        pe_event(a,(pe_event_t)which,caps,now);
        baseline_pe_event(b,(pe_event_t)which,caps,now);
    } else if (which==14u) {
        pe_tick(a,now);baseline_pe_tick(b,now);
    } else if (which==15u) {
        pe_command_result(a,true,now);baseline_pe_command_result(b,true,now);
    } else if (which==16u) {
        pe_command_result(a,false,now);baseline_pe_command_result(b,false,now);
    } else if (which==17u) {
        x=pe_request_soft_reset(a,now);y=baseline_pe_request_soft_reset(b,now);
        if (x!=y) abort();
    } else {
        x=pe_request_source_caps(a);y=baseline_pe_request_source_caps(b);
        if (x!=y) abort();
    }
    compare(a,b,seed,step);
}
int main(void) {
    for (unsigned seed=1u;seed<=32u;seed++) {
        pe_t a,b;
        pe_init(&a);baseline_pe_init(&b);
        compare(&a,&b,seed,0);
        pe_caps_t caps=caps_for(0);
        unsigned step=0;
        uint32_t now=seed==1u?0xfffffff0u:seed*111u;
        #define RUN(X) step_pair(&a,&b,(X),now,&caps,seed,++step)
        RUN(PE_EV_ATTACH); now+=3u;RUN(PE_EV_CAPS);
        now+=1u;RUN(15u);now+=1u;RUN(PE_EV_ACCEPT);
        now+=1u;RUN(PE_EV_PS_RDY);
        now+=5000u;RUN(14u);now+=1u;RUN(15u);
        now+=1u;RUN(PE_EV_WAIT);
        now+=120u;RUN(14u);
        now+=1u;RUN(PE_EV_HARD_RESET_RX);
        now+=3u;RUN(PE_EV_HR_VBUS_READY);
        now+=3u;RUN(PE_EV_CAPS);
        now+=1u;RUN(15u);now+=1u;RUN(PE_EV_ACCEPT);
        now+=1u;RUN(PE_EV_PS_RDY);
        now+=1u;RUN(PE_EV_SENSOR_FAULT);
        now+=2u;RUN(PE_EV_DETACH);
        #undef RUN
        rng=0x9e3779b9u^(seed*0x00baddadu);
        for (unsigned i=0u;i<10000u;i++) {
            unsigned op=rnd()%19u;
            now+=rnd()%9000u;
            caps=caps_for(rnd());
            step_pair(&a,&b,op,now,&caps,seed,++step);
        }
    }
    puts("POLICY_PARITY_PASS 32 seeds x 10000 random + directed sequences");
    return 0;
}
