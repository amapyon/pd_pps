#ifndef M8D_TRACE_H
#define M8D_TRACE_H

#include <stdint.h>

/* SRAM-only diagnostic ABI. For this experiment, all words are LE uint32_t.
 * Never change field order without changing ABI version and decoder. */
#define M8D_TRACE_MAGIC    0x4d385452u /* 'M8TR' */
#define M8D_TRACE_VERSION  1u
#define M8D_TRACE_CAPACITY 32u

#define M8D_EV_TX_BEGIN      1u /* foreground, before USBFS_SendEndpoint */
#define M8D_EV_TX_ACCEPTED   2u /* foreground, SendEndpoint returned 0 */
#define M8D_EV_TX_REJECTED   3u /* foreground, negative SendEndpoint result */
#define M8D_EV_WAIT_SAMPLE   4u /* foreground, EP3 outstanding sample */
#define M8D_EV_WAIT_END      5u /* foreground, outstanding interval ended */
#define M8D_EV_EP3_IRQ_IN   16u /* callback DURING IN IRQ, before library clears busy */
#define M8D_EV_RX_EP2       17u /* callback DURING EP2 OUT IRQ */
#define M8D_EV_CTRL_SETUP   18u /* callback DURING class-specific setup */

/* 36-byte record (9 x 32-bit LE). Extract with <9I from SRAM.
 * usb_flags: INT_FG | INT_ST<<8 | BASE_CTRL<<16 | UDEV_CTRL<<24
 * ep3_regs: EP3_TX_LEN | EP3_TX_CTRL<<8 | INT_EN<<16 | UEP2_3_MOD<<24
 * ctx_flags: EP3.busy | EP3.mode<<8 | SetupReqLen<<16 | errata_window<<31
 * event: see M8D_EV_*; arg: TX length/rc, wait iterations, RX length or setup.
 */
typedef struct {
    uint32_t sequence;
    uint32_t event;
    uint32_t main_loops;
    uint32_t arg;
    uint32_t usb_flags;
    uint32_t ep3_regs;
    uint32_t ctx_flags;
    uint32_t tx_accepted;
    uint32_t ep3_in_completed;
} M8D_TraceEvent;

/* Rings intentionally have separate writers: foreground vs USB IRQ.
 * event slot is written before count is advanced; WCH-LinkE must halt MCU
 * before decoding for a consistent snapshot. Last 32 events per ring survive.
 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t struct_size;
    uint32_t event_size;
    uint32_t capacity;
    uint32_t fg_count;
    uint32_t irq_count;
    uint32_t main_loops;
    uint32_t outstanding_iterations;
    uint32_t max_outstanding_iterations;
    uint32_t last_usb_flags;
    uint32_t last_ep3_regs;
    uint32_t last_ctx_flags;
    uint32_t last_tx_accepted;
    uint32_t last_ep3_in_completed;
    M8D_TraceEvent fg_events[M8D_TRACE_CAPACITY];
    M8D_TraceEvent irq_events[M8D_TRACE_CAPACITY];
} M8D_UsbTrace;

extern volatile M8D_UsbTrace m8d_usb_trace;

void M8D_TraceInit(void);
void M8D_TraceFG(uint32_t event, uint32_t arg,
                 uint32_t tx_accepted, uint32_t ep3_in_completed);
void M8D_TraceIRQ(uint32_t event, uint32_t arg,
                  uint32_t tx_accepted, uint32_t ep3_in_completed);
void M8D_TracePoll(uint32_t tx_accepted, uint32_t ep3_in_completed);

#endif
