#include "m28_transport.h"
#include <string.h>

void m28_transport_init(m28_transport_t *t) {
    if (t) memset(t, 0, sizeof(*t));
}
void m28_transport_rx_irq(m28_transport_t *t, const uint8_t *data, size_t length) {
    if (!t || !data) return;
    for (size_t i = 0; i < length; ++i) {
        uint8_t next = (uint8_t)(t->head + 1u);
        if (next == t->tail) {
            t->overflow = 1u;
            t->overflow_count++;
            return; /* Never overwrite unread bytes. */
        }
        t->data[t->head] = data[i];
        t->head = next; /* Publish only after writing the payload. */
    }
}
void m28_transport_bus_reset_irq(m28_transport_t *t) {
    if (t) t->bus_reset_seq++;
}
void m28_transport_in_complete_irq(m28_transport_t *t) {
    if (t) t->in_complete_seq++;
}
bool m28_transport_pop(m28_transport_t *t, uint8_t *out) {
    if (!t || !out || t->tail == t->head) return false;
    *out = t->data[t->tail];
    t->tail = (uint8_t)(t->tail + 1u);
    return true;
}
void m28_transport_discard(m28_transport_t *t) {
    if (!t) return;
    t->tail = t->head;
    t->overflow = 0;
}
