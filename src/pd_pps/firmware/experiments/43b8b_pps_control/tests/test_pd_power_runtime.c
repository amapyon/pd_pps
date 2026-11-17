/* Pure power planner tests: no MCU, PHY, or IO dependencies. */
#include "pd_power.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define FIXED_5V(ma10) ((100u << 10) | (ma10))
#define APDO(min100,max100,ma50) ((3u << 30) | ((max100) << 17) | ((min100) << 8) | (ma50))
#define RDO_PPS(pos,mv,ma) (((uint32_t)(pos) << 28) | (1u<<25) | (1u<<24) | (((mv)/20u)<<9) | ((ma)/50u))
#define RDO_FIXED (((uint32_t)1u << 28) | (1u<<25) | (1u<<24) | (100u<<10) | 100u)

static void check_select(const uint32_t *caps, uint8_t n, uint32_t mv,
                         uint32_t ma, pd_power_result_t expect,
                         uint8_t pos, uint32_t rdo, bool pps)
{
    pd_power_plan_t p={0x12345678u,0x5au,true};
    pd_power_result_t got=pd_power_select(caps,n,mv,ma,&p);
    assert(got==expect);
    if (got==PD_POWER_PPS || got==PD_POWER_FIXED_5V) {
        assert(p.object_pos==pos);
        assert(p.rdo==rdo);
        assert(p.pps==pps);
    } else {
        assert(p.rdo==0x12345678u && p.object_pos==0x5au && p.pps);
    }
}
int main(void) {
    const uint32_t pps[7]={FIXED_5V(300u),APDO(33u,110u,60u)};
    const uint32_t no_pps[7]={FIXED_5V(300u)};
    const uint32_t low_fixed[7]={FIXED_5V(90u)};
    const uint32_t low_fixed_with_pps[7]={FIXED_5V(90u),APDO(33u,110u,60u)};
    const uint32_t invalid_first[7]={((180u<<10)|300u),APDO(33u,110u,60u)};
    const uint32_t other_apdo[7]={FIXED_5V(300u),APDO(33u,110u,60u)|(1u<<28)};
    const uint32_t small_range[7]={FIXED_5V(300u),APDO(33u,60u,60u)};
    const uint32_t weak_pps[7]={FIXED_5V(300u),APDO(33u,110u,19u)};
    const uint32_t priority[7]={FIXED_5V(300u),APDO(33u,110u,19u),APDO(33u,110u,60u)};
    const uint32_t first_match[7]={FIXED_5V(300u),APDO(33u,110u,60u),APDO(33u,110u,70u)};

    check_select(pps,2u,7000u,1000u,PD_POWER_PPS,2u,RDO_PPS(2u,7000u,1000u),true);
    check_select(no_pps,1u,7000u,1000u,PD_POWER_FIXED_5V,1u,RDO_FIXED,false);
    check_select(low_fixed,1u,7000u,1000u,PD_POWER_UNAVAILABLE,0,0,false);
    /* A low-current fixed PDO is not a blocker when a valid PPS APDO exists. */
    check_select(low_fixed_with_pps,2u,7000u,1000u,PD_POWER_PPS,2u,RDO_PPS(2u,7000u,1000u),true);
    check_select(invalid_first,2u,7000u,1000u,PD_POWER_INVALID_CAPS,0,0,false);
    check_select(other_apdo,2u,7000u,1000u,PD_POWER_FIXED_5V,1u,RDO_FIXED,false);
    check_select(small_range,2u,7000u,1000u,PD_POWER_FIXED_5V,1u,RDO_FIXED,false);
    check_select(weak_pps,2u,7000u,1000u,PD_POWER_FIXED_5V,1u,RDO_FIXED,false);
    check_select(priority,3u,7000u,1000u,PD_POWER_PPS,3u,RDO_PPS(3u,7000u,1000u),true);
    check_select(first_match,3u,7000u,1000u,PD_POWER_PPS,2u,RDO_PPS(2u,7000u,1000u),true);
    check_select(pps,0u,7000u,1000u,PD_POWER_INVALID_CAPS,0,0,false);
    check_select(pps,8u,7000u,1000u,PD_POWER_INVALID_CAPS,0,0,false);
    check_select(NULL,2u,7000u,1000u,PD_POWER_INVALID_CAPS,0,0,false);
    assert(pd_power_select(pps,2u,7000u,1000u,NULL)==PD_POWER_INVALID_CAPS);
    check_select(pps,2u,7001u,1000u,PD_POWER_INVALID_TARGET,0,0,false);
    check_select(pps,2u,7000u,1001u,PD_POWER_INVALID_TARGET,0,0,false);
    check_select(pps,2u,0u,1000u,PD_POWER_INVALID_TARGET,0,0,false);
    check_select(pps,2u,7000u,0u,PD_POWER_INVALID_TARGET,0,0,false);
    check_select(pps,2u,7000u,6400u,PD_POWER_INVALID_TARGET,0,0,false);
    /* The pure plan can encode another representable PPS target. */
    check_select(pps,2u,7100u,1050u,PD_POWER_PPS,2u,RDO_PPS(2u,7100u,1050u),true);
    /* Fixed-5V fallback is pinned to 1 A even for other target values. */
    check_select(no_pps,1u,7100u,1050u,PD_POWER_FIXED_5V,1u,RDO_FIXED,false);
    puts("POWER_SELECT_PASS");
    return 0;
}
