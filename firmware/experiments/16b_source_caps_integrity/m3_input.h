#ifndef M3_INPUT_H
#define M3_INPUT_H

#include <stdbool.h>
#include <stdint.h>

/* Pure state machine: no MCU SDK, no interrupts, no PPS changes. */
#define M3_BUTTON_DEBOUNCE_MS 25u
#define M3_QUADRATURE_EDGES_PER_DETENT 4  /* Change to 2 only after EC11 characterization. */
#define M3_POLL_PERIOD_MS 2u

typedef struct {
    bool a_high;
    bool b_high;
    bool encoder_sw_high;
    bool back_high;
} m3_levels_t;

typedef struct {
    bool stable_high;
    bool candidate_high;
    uint32_t candidate_since_ms;
} m3_button_t;

typedef struct {
    uint8_t previous_ab;
    int8_t edge_accumulator;
    int32_t position;
    uint32_t cw_count;
    uint32_t ccw_count;
    uint32_t glitch_count;
    uint32_t press_count;
    uint32_t back_count;
    m3_button_t encoder_sw;
    m3_button_t back;
} m3_input_t;

typedef struct {
    int8_t rotation;      /* +1 for A-first sequence, -1 for B-first; name is wiring-dependent. */
    bool encoder_press;   /* debounced HIGH->LOW transition */
    bool back_press;      /* debounced HIGH->LOW transition */
} m3_events_t;

void m3_input_init(m3_input_t *s, m3_levels_t initial, uint32_t now_ms);
m3_events_t m3_input_sample(m3_input_t *s, m3_levels_t raw, uint32_t now_ms);

#endif
