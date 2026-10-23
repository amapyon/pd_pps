#include "../pps_apdo_limits.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    PPSApdoLimits p;
    /* Real UGREEN 200W report: PDO[5] 0xC1A4213C */
    assert(pps_apdo_decode_raw(0xC1A4213Cu, &p));
    assert(p.min_mv == 3300u && p.max_mv == 21000u && p.max_ma == 3000u);
    assert(pps_apdo_accepts(p, 3300u, 50u));
    assert(pps_apdo_accepts(p, 21000u, 3000u));
    assert(pps_apdo_accepts(p, 7000u, 1800u));
    assert(!pps_apdo_accepts(p, 3280u, 1000u));
    assert(!pps_apdo_accepts(p, 21020u, 1000u));
    assert(!pps_apdo_accepts(p, 7000u, 3050u));
    assert(!pps_apdo_accepts(p, 7010u, 1000u));
    assert(!pps_apdo_accepts(p, 7000u, 1025u));
    assert(!pps_apdo_accepts(p, 7000u, 0u));
    /* Fixed 20V/3.25A PDO cannot change PPS 3.0A maximum. */
    assert(!pps_apdo_decode_raw(0x00064145u, &p));
    /* Wrong augmented subtype is not SPR PPS. */
    assert(!pps_apdo_decode_raw(0xD1A4213Cu, &p));
    /* Multiple APDOs: a V/I pair must belong to the same APDO. */
    PPSApdoLimits a = {3300u, 11000u, 3000u};
    PPSApdoLimits b = {11000u, 21000u, 1500u};
    assert(pps_apdo_accepts(a, 10000u, 3000u));
    assert(pps_apdo_accepts(b, 20000u, 1500u));
    assert(!pps_apdo_accepts(a, 20000u, 3000u));
    assert(!pps_apdo_accepts(b, 20000u, 3000u));
    puts("APDO runtime limit rules and UGREEN PDO[5] decode OK");
    return 0;
}
