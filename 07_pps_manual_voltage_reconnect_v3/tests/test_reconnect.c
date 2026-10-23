#include "../reconnect_policy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    CCDetachFilter f;
    cc_detach_filter_clear(&f);
    assert(!cc_detach_filter_update(&f, true, 100u, 150u));
    assert(!cc_detach_filter_update(&f, false, 100u, 150u));
    assert(!cc_detach_filter_update(&f, false, 249u, 150u));
    assert(cc_detach_filter_update(&f, false, 250u, 150u));
    assert(!cc_detach_filter_update(&f, true, 260u, 150u));
    assert(!cc_detach_filter_update(&f, false, 300u, 150u));
    assert(!cc_detach_filter_update(&f, true, 320u, 150u));
    /* Wrap-around should not defeat the debounce. */
    cc_detach_filter_clear(&f);
    assert(!cc_detach_filter_update(&f, false, UINT32_MAX-99u, 150u));
    assert(!cc_detach_filter_update(&f, false, 30u, 150u));
    assert(cc_detach_filter_update(&f, false, 51u, 150u));
    puts("reconnect filter test OK");
    return 0;
}
