#ifndef M28_TRANSPORT_H
#define M28_TRANSPORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Single ISR producer, foreground consumer; 256-byte circular buffer.
 * Volatile index writes are byte-atomic on RV32. No heap or blocking. */
typedef struct {
    uint8_t data[256];
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t overflow;
    volatile uint32_t bus_reset_seq;
    volatile uint32_t in_complete_seq;
    volatile uint32_t overflow_count;
} m28_transport_t;

void m28_transport_init(m28_transport_t *t);
void m28_transport_rx_irq(m28_transport_t *t, const uint8_t *data, size_t length);
void m28_transport_bus_reset_irq(m28_transport_t *t);
void m28_transport_in_complete_irq(m28_transport_t *t);
bool m28_transport_pop(m28_transport_t *t, uint8_t *out);
void m28_transport_discard(m28_transport_t *t);
#endif
