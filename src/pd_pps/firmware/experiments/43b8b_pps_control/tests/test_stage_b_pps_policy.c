#include "pd_policy.h"
#include "pd_power.h"
#include <assert.h>
#include <stdio.h>
#define FIXED_5V(ma10) ((100u << 10) | (ma10))
#define APDO(min100,max100,ma50) ((3u << 30) | ((max100) << 17) | ((min100) << 8) | (ma50))
#define PPS_RDO(pos,mv,ma) (((uint32_t)(pos) << 28) | (1u<<25) | (1u<<24) | (((mv)/20u)<<9) | ((ma)/50u))
static void enter_ready(pe_t *p, const pe_caps_t *caps) {
    pe_init(p);
    pe_event(p,PE_EV_ATTACH,NULL,10u);
    pe_event(p,PE_EV_CAPS,caps,20u);
    assert(p->state==PE_WAIT_ACCEPT);
    pe_command_result(p,true,21u);
    pe_event(p,PE_EV_ACCEPT,NULL,22u);
    pe_event(p,PE_EV_PS_RDY,NULL,23u);
    assert(p->state==PE_READY && p->contract_valid && p->pps_active);
    assert(p->requested_mv==7000u && p->requested_ma==1000u);
}
int main(void) {
    pe_caps_t caps={0}; caps.count=2;
    caps.pdo[0]=FIXED_5V(300u);
    caps.pdo[1]=APDO(33u,110u,60u);
    pe_t p;
    enter_ready(&p,&caps);
    assert(pe_target_mv(&p)==7000u && pe_target_ma(&p)==1000u);
    assert(pe_set_pps_target(&p,7000u,1000u,24u)==PE_PPS_UNCHANGED);
    assert(pe_set_pps_target(&p,7001u,1000u,24u)==PE_PPS_BAD_RANGE);
    assert(pe_set_pps_target(&p,8001u,1000u,24u)==PE_PPS_BAD_RANGE);
    assert(pe_set_pps_target(&p,7000u,1050u,24u)==PE_PPS_BAD_RANGE);
    assert(pe_set_pps_target(&p,4980u,1000u,24u)==PE_PPS_BAD_RANGE);
    assert(pe_set_pps_target(&p,7020u,1000u,30u)==PE_PPS_QUEUED);
    assert(p.state==PE_WAIT_ACCEPT && p.command==PE_CMD_REQUEST);
    assert(p.rdo==PPS_RDO(2u,7020u,1000u));
    assert(p.requested_mv==7000u && pe_target_mv(&p)==7020u);
    assert(pe_set_pps_target(&p,7040u,1000u,30u)==PE_PPS_BUSY);
    pe_command_result(&p,true,31u);
    pe_event(&p,PE_EV_ACCEPT,NULL,32u);
    assert(p.state==PE_WAIT_PS_RDY && p.requested_mv==7000u);
    pe_event(&p,PE_EV_PS_RDY,NULL,33u);
    assert(p.state==PE_READY && p.requested_mv==7020u);
    assert(pe_set_pps_target(&p,7000u,1000u,40u)==PE_PPS_QUEUED);
    pe_command_result(&p,true,41u);
    pe_event(&p,PE_EV_ACCEPT,NULL,42u);
    pe_event(&p,PE_EV_PS_RDY,NULL,43u);
    assert(p.state==PE_READY && p.requested_mv==7000u);
    /* Manual request requires an APDO; never silently fall back to fixed. */
    caps.pdo[1]=APDO(33u,60u,60u);
    enter_ready(&p,&(pe_caps_t){.count=2,.pdo={FIXED_5V(300u),APDO(33u,110u,60u)}});
    p.caps=caps;
    assert(pe_set_pps_target(&p,7020u,1000u,50u)==PE_PPS_UNSUPPORTED);
    assert(p.state==PE_READY && p.command==PE_CMD_NONE && p.target_mv==0u);
    enter_ready(&p,&(pe_caps_t){.count=2,.pdo={FIXED_5V(300u),APDO(33u,110u,60u)}});
    assert(pe_set_pps_target(&p,7020u,1000u,50u)==PE_PPS_QUEUED);
    pe_event(&p,PE_EV_TX_FAILED,NULL,51u);
    assert(p.state==PE_RECOVERY && !p.contract_valid);
    assert(p.requested_mv==7000u); /* Old contract still recorded, not falsely committed. */
    assert(pe_set_pps_target(&p,7060u,1000u,52u)==PE_PPS_NOT_READY);
    pe_event(&p,PE_EV_DETACH,NULL,53u);
    assert(p.state==PE_DETACHED && pe_target_mv(&p)==7000u);
    puts("STAGE_B_PPS_POLICY_PASS");
    return 0;
}
