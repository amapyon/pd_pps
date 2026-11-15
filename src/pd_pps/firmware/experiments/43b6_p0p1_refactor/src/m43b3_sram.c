/* M43B-3 SRAM event journal. No print/heap/PD register writes.
 * One producer at a time: USBPD ISR or foreground with USBPD_IRQn masked.
 * USBFS IRQ is NOT a producer, avoiding concurrent writes. */
#include "m43b3_sram.h"

/* NOT in .noinit: new firmware boot starts with an empty diagnostic state.
 * Keep WCH-LinkE power and avoid any reset between fault and SRAM dump.
 * This global symbol is resolved via riscv-none-elf-nm for the decoder. */
__attribute__((used, aligned(4))) volatile m43b3_sram_t m43b3_sram;

static uint32_t next_session;

typedef char m43b3_entry_size_check[(sizeof(m43b3_sram_entry_t)==16u)?1:-1];
typedef char m43b3_header_size_check[(sizeof(m43b3_sram_t)==1088u)?1:-1];

void m43b3_sram_attach(uint32_t tick) {
    uint32_t s = next_session + 1u;
    next_session = s;
    m43b3_sram.magic = 0u;
    m43b3_sram.version = M43B3_SRAM_VERSION;
    m43b3_sram.header_bytes = M43B3_SRAM_HEADER_BYTES;
    m43b3_sram.record_bytes = M43B3_SRAM_RECORD_BYTES;
    m43b3_sram.capacity = M43B3_SRAM_CAPACITY;
    m43b3_sram.session = s;
    m43b3_sram.head = 0u;
    m43b3_sram.count = 0u;
    m43b3_sram.total = 0u;
    m43b3_sram.freeze_reason = M43B3_FREEZE_NONE;
    m43b3_sram.freeze_tick = 0u;
    m43b3_sram.active = 1u;
    m43b3_sram.lost = 0u;
    m43b3_sram.reserved0 = tick; /* attach SysTick low 32 bits */
    m43b3_sram.reserved1 = 0u;
    m43b3_sram.end_magic = M43B3_SRAM_END;
    __asm__ volatile("" ::: "memory");
    m43b3_sram.magic = M43B3_SRAM_MAGIC;
}
void m43b3_sram_note(uint8_t event, uint8_t flags,
                     uint32_t arg0, uint32_t arg1, uint32_t tick) {
    if (m43b3_sram.magic != M43B3_SRAM_MAGIC ||
        !m43b3_sram.active || m43b3_sram.freeze_reason != M43B3_FREEZE_NONE)
        return;
    uint32_t slot = m43b3_sram.head;
    volatile m43b3_sram_entry_t *p = &m43b3_sram.records[slot];
    p->tick = tick;
    p->arg0 = arg0;
    p->arg1 = arg1;
    p->seq = (uint16_t)(m43b3_sram.total & 0xffffu);
    p->event = event;
    p->flags = flags;
    __asm__ volatile("" ::: "memory");
    m43b3_sram.head = (slot + 1u) & (M43B3_SRAM_CAPACITY - 1u);
    if (m43b3_sram.count < M43B3_SRAM_CAPACITY) m43b3_sram.count++;
    m43b3_sram.total++;
}
void m43b3_sram_trace(uint8_t pdtr_kind, uint8_t detail,
                      uint16_t aux, uint32_t value, uint32_t tick) {
    /* Preserve high-value events; skip PDO payload and verbose tick events. */
    switch (pdtr_kind) {
        case 1u: case 2u: case 3u: case 4u: case 6u:
        case 7u: case 9u: case 10u: case 11u: case 12u:
        case 13u: case 14u: case 15u: case 17u: case 18u:
        case 28u:
            m43b3_sram_note(M43B3_EVT_TRACE, pdtr_kind,
                             (uint32_t)detail | ((uint32_t)aux << 8), value, tick);
            break;
        default: break;
    }
}
void m43b3_sram_freeze(uint32_t reason, uint32_t tick) {
    if (!m43b3_sram.active || m43b3_sram.magic != M43B3_SRAM_MAGIC ||
        m43b3_sram.freeze_reason != M43B3_FREEZE_NONE) return;
    m43b3_sram.freeze_tick = tick;
    __asm__ volatile("" ::: "memory");
    m43b3_sram.freeze_reason = reason; /* commit marker, set last */
    m43b3_sram.active = 0u;
}
void m43b3_sram_detach(uint32_t tick) {
    m43b3_sram_freeze(M43B3_FREEZE_DETACH, tick);
}
