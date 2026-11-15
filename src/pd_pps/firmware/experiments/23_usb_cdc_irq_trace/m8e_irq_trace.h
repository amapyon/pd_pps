#ifndef M8E_IRQ_TRACE_H
#define M8E_IRQ_TRACE_H
#include <stdint.h>
#define M8E_IRQ_MAGIC     0x4d384952u /* M8IR */
#define M8E_IRQ_VERSION   1u
#define M8E_IRQ_CAPACITY  64u
/* USBFS IRQ entry record, 8 x little-endian uint32 words, 32 bytes.
 * Note: USBFS combined INT_FG/INT_ST is captured before fsusb.c handles it.
 * m8d_irq_callbacks is the total experiment-22 callback trace count at IRQ entry.
 * These are state snapshots, NOT timestamps or wire-level packet traces.
 */
typedef struct {
    uint32_t sequence;
    uint32_t main_loops;
    uint32_t combined_flags;
    uint32_t ep3_regs;
    uint32_t usb_regs;
    uint32_t ctx_flags;
    uint32_t m8d_fg_events;
    uint32_t m8d_irq_callbacks;
} M8E_IrqEvent;
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t struct_size;
    uint32_t event_size;
    uint32_t capacity;
    uint32_t irq_total;
    uint32_t transfer_total;
    uint32_t ep3_in_total;
    uint32_t ep3_in_tog_ok;
    uint32_t ep3_in_tog_bad;
    uint32_t ep2_out_total;
    uint32_t ep0_setup_total;
    uint32_t bus_reset_total;
    uint32_t suspend_total;
    uint32_t other_total;
    uint32_t last_combined;
    uint32_t last_ep3;
    uint32_t last_context;
    M8E_IrqEvent events[M8E_IRQ_CAPACITY];
} M8E_IrqTrace;
extern volatile M8E_IrqTrace m8e_irq_trace;
void M8E_IrqTraceInit(void);
void M8E_IrqEntry(uint16_t combined_flags);
#endif
