#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_caps_integrity.h"
int main(void)
{
    const uint32_t c1[] = {
        0x0801912Cu, 0x0002D12Cu, 0x0003C12Cu,
        0x0004B12Cu, 0x000641F4u, 0xC1A42164u
    };
    const uint32_t dual[] = {
        0x0801912Cu, 0x0002D12Cu, 0x0003C12Cu,
        0x0004B12Cu, 0x00164145u, 0xC8DC215Au
    };
    const uint32_t c2[] = {
        0x0801912Cu, 0x0002D12Cu, 0x0003C0FAu, 0xC8DC2137u
    };
    uint32_t broken[] = {
        0x0801912Cu, 0x0002D12Cu, 0x0003C12Cu,
        0x0004B12Cu, 0x00164145u, 0x2F9AD2C3u
    };
    assert(pd_caps_frame_length_valid(6u, 30u));
    assert(pd_caps_frame_length_valid(4u, 22u));
    assert(!pd_caps_frame_length_valid(6u, 26u)); /* truncated */
    assert(!pd_caps_frame_length_valid(8u, 38u)); /* exceeds SPR max */
    assert(!pd_caps_frame_length_valid(0u, 6u));
    assert(pd_caps_words_plausible(c1, 6u));
    assert(pd_caps_words_plausible(dual, 6u));
    assert(pd_caps_words_plausible(c2, 4u));
    assert(!pd_caps_words_plausible(broken, 6u)); /* observed anomalous word */
    broken[5] = 0xC8DC215Au;
    broken[0] = 0u;
    assert(!pd_caps_words_plausible(broken, 6u));
    puts("PASS: Source_Capabilities frame length and PDO sanity checks");
    return 0;
}
