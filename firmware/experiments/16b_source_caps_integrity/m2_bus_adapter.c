#include "m2_bus_adapter.h"

bool m2_bus_write(uint8_t address7, const uint8_t *bytes,
                  size_t length, void *context)
{
    m2_i2c_ops_t *bus = (m2_i2c_ops_t *)context;
    bool ok;
    size_t i;
    if (!bus || !bus->start || !bus->write_byte || !bus->stop ||
        !bytes || address7 > 0x7Fu) return false;
    /* BEGIN: all calls are synchronous and must be from the PD main loop. */
    ok = bus->start(bus->user);
    if (ok) ok = bus->write_byte((uint8_t)(address7 << 1), bus->user);
    for (i = 0; i < length && ok; ++i) {
        ok = bus->write_byte(bytes[i], bus->user);
    }
    bus->stop(bus->user);  /* STOP even on a NACK to avoid leaving SDA low */
    return ok;
}
