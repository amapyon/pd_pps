#include "m3_input.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

/* Clockwise polarity is provisional: A-low first maps to +1. */
static const int8_t gray_delta[16] = {
    0, +1, -1,  0,
   -1,  0,  0, +1,
   +1,  0,  0, -1,
    0, -1, +1,  0
};

static uint8_t ab_state(m3_levels_t raw)
{
    return (uint8_t)((raw.a_high ? 1u : 0u) | (raw.b_high ? 2u : 0u));
}

static void button_init(m3_button_t *b, bool high, uint32_t now_ms)
{
    b->stable_high = high;
    b->candidate_high = high;
    b->candidate_since_ms = now_ms;
}

static bool button_pressed(m3_button_t *b, bool high, uint32_t now_ms)
{
    if (high != b->candidate_high) {
        b->candidate_high = high;
        b->candidate_since_ms = now_ms;
    }
    if (b->stable_high != b->candidate_high &&
        (uint32_t)(now_ms - b->candidate_since_ms) >= M3_BUTTON_DEBOUNCE_MS) {
        b->stable_high = b->candidate_high;
        return !b->stable_high;  /* only report press, not release */
    }
    return false;
}

void m3_input_init(m3_input_t *s, m3_levels_t initial, uint32_t now_ms)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->previous_ab = ab_state(initial);
    button_init(&s->encoder_sw, initial.encoder_sw_high, now_ms);
    button_init(&s->back, initial.back_high, now_ms);
}

m3_events_t m3_input_sample(m3_input_t *s, m3_levels_t raw, uint32_t now_ms)
{
    m3_events_t result = {0};
    if (!s) return result;
    const uint8_t current = ab_state(raw);
    const uint8_t old = s->previous_ab;
    s->previous_ab = current;
    if (old != current) {
        const int8_t d = gray_delta[(old << 2u) | current];
        if (!d) {
            /* Both bits jumped: missed samples or wiring fault. Resynchronize. */
            s->edge_accumulator = 0;
            ++s->glitch_count;
        } else {
            /* Reversal chatter cancels rather than generating extra clicks. */
            s->edge_accumulator = (int8_t)(s->edge_accumulator + d);
            if (s->edge_accumulator >= M3_QUADRATURE_EDGES_PER_DETENT) {
                s->edge_accumulator = 0;
                if (s->position < INT32_MAX) ++s->position;
                ++s->cw_count;
                result.rotation = +1;
            } else if (s->edge_accumulator <= -M3_QUADRATURE_EDGES_PER_DETENT) {
                s->edge_accumulator = 0;
                if (s->position > INT32_MIN) --s->position;
                ++s->ccw_count;
                result.rotation = -1;
            }
        }
    }
    if (button_pressed(&s->encoder_sw, raw.encoder_sw_high, now_ms)) {
        ++s->press_count;
        result.encoder_press = true;
    }
    if (button_pressed(&s->back, raw.back_high, now_ms)) {
        ++s->back_count;
        result.back_press = true;
    }
    return result;
}
