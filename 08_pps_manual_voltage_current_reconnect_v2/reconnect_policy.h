#ifndef RECONNECT_POLICY_H
#define RECONNECT_POLICY_H

#include <stdbool.h>
#include <stdint.h>

/* Ignore short CC comparator glitches. Caller samples at a periodic interval.
 * Unsigned subtraction also handles uint32_t SysTick wraparound.
 */
typedef struct {
    uint32_t absent_since_ms;
    bool absence_pending;
} CCDetachFilter;

static inline void cc_detach_filter_clear(CCDetachFilter *filter)
{
    filter->absent_since_ms = 0u;
    filter->absence_pending = false;
}

static inline bool cc_detach_filter_update(CCDetachFilter *filter,
                                           bool cc_present,
                                           uint32_t now_ms,
                                           uint32_t debounce_ms)
{
    if (cc_present) {
        cc_detach_filter_clear(filter);
        return false;
    }
    if (!filter->absence_pending) {
        filter->absent_since_ms = now_ms;
        filter->absence_pending = true;
        return false;
    }
    return (uint32_t)(now_ms - filter->absent_since_ms) >= debounce_ms;
}

#endif
