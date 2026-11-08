/* CH32X035 USBPD PHY adapter. Hardware/board validation required.
 * This file does not implement full USB PD protocol conformance.
 * It confines IRQ, CC, GoodCRC and DMA accesses to one translation unit.
 */
#include "ch32fun.h"
#include "pd_phy_ch32x035.h"
#include <string.h>

#define Q_LEN 12u
#define TRACE_LEN 128u  /* 127 records, first events retained if full. */
#define CC_CONFIRM_MS 20u
#define CC_DETACH_MS 150u
/* PD 3.2 §7.31.1: CRCReceiveTimer begins at TX EOP, not TX start.
   tReceive nominal is 1000 us (900-1100 us; Table 7.9).
   TX_END is the CH32X035 hardware completion indication. */
#define CRC_RECEIVE_TICKS (FUNCONF_SYSTEM_CORE_CLOCK / 1000u)
#define PD_CTRL_GOODCRC 1u
#define PD_CTRL_ACCEPT 3u
#define PD_CTRL_REJECT 4u
#define PD_CTRL_PS_RDY 6u
#define PD_CTRL_WAIT 12u
#define PD_CTRL_GET_SOURCE_CAP 7u
#define PD_CTRL_SOFT_RESET 13u
#define PD_DATA_SOURCE_CAP 1u
#define PD_DATA_REQUEST 2u
/* CH32X035 RM §21.2.7: TX_SEL1=RST1, TX_SEL2/3=RST1,
 * TX_SEL4=RST2. Ordered set is RST1,RST1,RST1,RST2 (§5.3.1.2.1).
 * Keep this independent of UPD_SOP0; it selects a non-SOP signal. */
#define PD_HARD_RESET_TX_SEL 0xa9u
/* 5ms tHardResetComplete max + 1ms diagnostic margin, not a retry. */
#define PD_HARD_RESET_TX_TIMEOUT_TICKS (6u * (FUNCONF_SYSTEM_CORE_CLOCK / 1000u))

typedef struct { pe_event_t type; pe_caps_t caps; } rx_event_t;
static __attribute__((aligned(4))) uint8_t dma_buffer[64];
static volatile rx_event_t queue[Q_LEN];
static volatile uint8_t qhead, qtail;
static volatile uint32_t lost;
static volatile bool attached, hard_reset, tx_wait_crc, vbus_loss_latched;
static volatile bool hard_reset_tx_active, hard_reset_tx_finished, hard_reset_tx_timeout;
static volatile uint32_t hard_reset_tx_start_tick;
static volatile bool hr_rx_early;
static bool hr_early_caps_valid;
static pe_caps_t hr_early_caps;
static volatile uint8_t tx_msg_id, tx_expected_id, last_rx_id, last_rx_valid;
static uint8_t retry_frame[6];
static uint8_t retry_size, retry_count;
static volatile uint32_t retry_eop_tick;
static volatile bool crc_wait_armed;
static volatile uint8_t selected_cc; /* 0=unselected, 1=CC1, 2=CC2 */
static uint32_t cc_changed_ms;
static bool cc_last;

/* Single-producer-at-a-time RAM journal (IRQ or IRQ-masked foreground).
 * No UART/debugprintf, allocation, or time division in the PD path.
 * Intentionally NOT cleared by phy_init(), which runs again on detach. */
static volatile pd_trace_t trace_buf[TRACE_LEN];
static volatile uint8_t trace_head, trace_tail;
static volatile uint32_t trace_drops;

static void trace_add(uint8_t kind, uint8_t detail, uint16_t aux, uint32_t value) {
    uint8_t next = (uint8_t)((trace_head + 1u) & (TRACE_LEN - 1u));
    if (next == trace_tail) { trace_drops++; return; }
    volatile pd_trace_t *t = &trace_buf[trace_head];
    t->tick = SysTick->CNTL;
    t->value = value;
    t->aux = aux;
    t->kind = kind;
    t->detail = detail;
    __asm volatile("" ::: "memory");
    trace_head = next;
}
void phy_trace_note(uint8_t kind, uint8_t detail, uint16_t aux, uint32_t value) {
    NVIC_DisableIRQ(USBPD_IRQn);
    trace_add(kind, detail, aux, value);
    if (attached) NVIC_EnableIRQ(USBPD_IRQn);
}
bool phy_trace_pop(pd_trace_t *out) {
    NVIC_DisableIRQ(USBPD_IRQn);
    if (trace_tail == trace_head) {
        if (attached) NVIC_EnableIRQ(USBPD_IRQn);
        return false;
    }
    const volatile pd_trace_t *t = &trace_buf[trace_tail];
    out->tick = t->tick;
    out->value = t->value;
    out->aux = t->aux;
    out->kind = t->kind;
    out->detail = t->detail;
    trace_tail = (uint8_t)((trace_tail + 1u) & (TRACE_LEN - 1u));
    if (attached) NVIC_EnableIRQ(USBPD_IRQn);
    return true;
}
uint32_t phy_trace_dropped(void) { return trace_drops; }


static void enqueue(pe_event_t type, const pe_caps_t *caps) {
    uint8_t n = (uint8_t)((qhead + 1u) % Q_LEN);
    if (n == qtail) { lost++; return; }
    volatile rx_event_t *e = &queue[qhead];
    e->type = type;
    e->caps.count = caps ? caps->count : 0u;
    e->caps.msg_id = caps ? caps->msg_id : 0u;
    if (caps) {
        for (uint8_t i = 0; i < caps->count && i < 7; i++) e->caps.pdo[i] = caps->pdo[i];
    }
    __asm volatile("" ::: "memory");
    qhead = n;
}
static void rx_mode(void) {
    USBPD->BMC_CLK_CNT = UPD_TMR_RX;
    USBPD->CONTROL = (USBPD->CONTROL & ~PD_TX_EN) | BMC_START;
}
static void transmit(uint8_t n) {
    USBPD->BMC_CLK_CNT = UPD_TMR_TX;
    USBPD->TX_SEL = UPD_SOP0;
    USBPD->BMC_TX_SZ = n;
    USBPD->CONTROL |= BMC_START | PD_TX_EN;
}
static uint16_t hdr(uint8_t type, uint8_t count, uint8_t mid) {
    /* UFP, Sink, PD 3.0, SOP */
    return (uint16_t)(type | (2u << 6) | ((uint16_t)(mid & 7u) << 9)
                          | ((uint16_t)count << 12));
}
static void good_crc(uint8_t msg_id) {
    uint16_t h = hdr(PD_CTRL_GOODCRC, 0, msg_id);
    memcpy(dma_buffer, &h, sizeof(h));
    transmit(2);
    /* Record after starting hardware GoodCRC transmission. */
    trace_add(PDTR_TX_GOODCRC, msg_id, h, 0);
}
static uint16_t load16(void) { uint16_t v; memcpy(&v, dma_buffer, 2); return v; }
static void receive_packet(void) {
    /* After Hard Reset, ignore stale SOP traffic until the coordinator
       deliberately arms the early-RX phase following stable low VBUS.
       Only early SourceCaps may be acknowledged and staged in that phase;
       an ordinary Request is blocked until stable 5V confirmation. */
    if (vbus_loss_latched || (hard_reset && !hr_rx_early)) return;
    uint8_t bytes = (uint8_t)USBPD->BMC_BYTE_CNT;
    if (bytes < 6u || bytes > sizeof(dma_buffer)) {
        trace_add(PDTR_RX_BAD_LENGTH, bytes, 0, 0);
        return;
    }
    uint16_t h = load16();
    uint8_t type = (uint8_t)(h & 0x1fu);
    uint8_t ndo = (uint8_t)((h >> 12) & 7u);
    uint8_t mid = (uint8_t)((h >> 9) & 7u);
    bool ext = (h & 0x8000u) != 0;
    if (ndo == 0u && type == PD_CTRL_GOODCRC) {
        uint8_t expected = tx_expected_id;
        bool match = tx_wait_crc && mid == expected;
        if (match) {
            tx_msg_id = (uint8_t)((tx_msg_id + 1u) & 7u);
            tx_wait_crc = false;
            crc_wait_armed = false;
            retry_count = 0;
        }
        trace_add(PDTR_RX_GOODCRC, mid, h,
                  (uint32_t)expected | ((uint32_t)match << 8));
        return;
    }
    /* Snapshot DMA before transmitting GoodCRC (which reuses DMA). */
    pe_caps_t caps = {0};
    bool is_caps = !ext && ndo && type == PD_DATA_SOURCE_CAP && ndo <= 7u &&
                   bytes == (uint8_t)(6u + ndo * 4u);
    if (is_caps) {
        caps.count = ndo; caps.msg_id = mid;
        for (uint8_t i = 0; i < ndo; i++) memcpy(&caps.pdo[i], &dma_buffer[2u + 4u * i], 4);
    }
    /* During Hard Reset recovery, only SourceCaps is valid provisional
       traffic. Acknowledge and hold it before the ADC triple-confirmation;
       never dispatch it to the PE or initiate a power Request this early. */
    if (hard_reset && hr_rx_early) {
        if (!is_caps) return;
        good_crc(mid);
        if (hr_early_caps_valid && hr_early_caps.msg_id == mid) {
            trace_add(PDTR_RX_DUPLICATE, mid, h, 0);
            return;
        }
        hr_early_caps = caps;
        hr_early_caps_valid = true;
        trace_add(PDTR_HR_EARLY_CAPS, mid, h, caps.count);
        return;
    }
    /* GoodCRC must be sent for valid SOP messages, including duplicates. */
    good_crc(mid);
    trace_add(PDTR_RX_HEADER, bytes, h, (uint32_t)is_caps);
    if (is_caps) {
        for (uint8_t i = 0; i < ndo; i++)
            trace_add(PDTR_RX_PDO, (uint8_t)(i + 1u), h, caps.pdo[i]);
    }
    if (last_rx_valid && last_rx_id == mid && type != PD_CTRL_SOFT_RESET) {
        trace_add(PDTR_RX_DUPLICATE, mid, h, 0);
        return;
    }
    /* M12d: a NEW SOP Message discards an outstanding SOP transmission.
       Otherwise a stale Request can be retransmitted after its AMS was
       superseded (USB PD 3.2 v1.2, sections 7.3 and 9.1.2.3.5).
       The Soft_Reset branch below resets all counters instead. */
    if (tx_wait_crc) {
        tx_wait_crc = false;
        crc_wait_armed = false;
        retry_count = 0;
        if (type != PD_CTRL_SOFT_RESET)
            tx_msg_id = (uint8_t)((tx_msg_id + 1u) & 7u);
        trace_add(PDTR_TX_DISCARDED, tx_expected_id, h, 0);
    }
    last_rx_valid = 1u; last_rx_id = mid;
    if (is_caps) enqueue(PE_EV_CAPS, &caps);
    else if (ndo == 0u) {
        if (type == PD_CTRL_ACCEPT) enqueue(PE_EV_ACCEPT, 0);
        if (type == PD_CTRL_PS_RDY) enqueue(PE_EV_PS_RDY, 0);
        if (type == PD_CTRL_REJECT) enqueue(PE_EV_REJECT, 0);
        if (type == PD_CTRL_WAIT) enqueue(PE_EV_WAIT, 0);
        if (type == PD_CTRL_SOFT_RESET) {
            tx_msg_id = 0; tx_wait_crc = false; crc_wait_armed = false;
            retry_count = 0; last_rx_valid = 0;
            enqueue(PE_EV_SOFT_RESET_RX, 0);
        }
    }
}
#if defined(__riscv)
void USBPD_IRQHandler(void) __attribute__((interrupt));
#endif
void USBPD_IRQHandler(void) {
    uint16_t status = (uint16_t)USBPD->STATUS;
    /* Hard Reset has priority: never GoodCRC a stale frame after reset. */
    if (status & IF_RX_RESET) {
        trace_add(PDTR_IRQ_RESET, 0, status, 0);
        hard_reset = true;
        hr_rx_early = false;
        hr_early_caps_valid = false;
        hard_reset_tx_active = false;
        tx_wait_crc = false; crc_wait_armed = false;
        tx_msg_id = 0; tx_expected_id = 0;
        last_rx_valid = 0; last_rx_id = 0;
        retry_count = 0; retry_size = 0; retry_eop_tick = 0;
        /* Prior RX events are no longer meaningful after Hard Reset.
           Flush the queue and make the reset event the next foreground
           event even if the old queue was full.  Foreground dequeue
           already masks USBPD_IRQn, so the reset is atomic to it. */
        qhead = qtail = 0;
        enqueue(PE_EV_HARD_RESET_RX, 0);
        USBPD->STATUS = IF_RX_RESET;
        if (status & IF_RX_ACT) USBPD->STATUS = IF_RX_ACT;
        if (status & IF_TX_END) USBPD->STATUS = IF_TX_END;
        rx_mode();
        return;
    }
    /* Experimental Sink-originated Hard Reset: TX_END closes the one
       ordered-set transmission. Do not arm GoodCRC retry or resume traffic. */
    if ((status & IF_TX_END) && hard_reset_tx_active) {
        hard_reset_tx_active = false;
        hard_reset_tx_finished = true;
        trace_add(PDTR_TX_HARD_RESET_END, 0, status, 0);
        USBPD->STATUS = IF_TX_END;
        if (status & IF_RX_ACT) USBPD->STATUS = IF_RX_ACT;
        rx_mode(); /* receive electronics only; SOP handler stays gated */
        return;
    }
    /* TX_END must be consumed before receiving a message that may cause
       a new GoodCRC TX; never switch to RX after starting that GoodCRC. */
    if (status & IF_TX_END) {
        trace_add(PDTR_IRQ_TX_END, 0, status, 0);
        /* Start CRCReceiveTimer only after EOP has left the PHY.
           When retrying, each transmitted frame starts a new timer. */
        if (tx_wait_crc && !crc_wait_armed) {
            retry_eop_tick = SysTick->CNTL;
            crc_wait_armed = true;
        }
        rx_mode();
        USBPD->STATUS = IF_TX_END; /* W1C */
    }
    if (status & IF_RX_ACT) {
        if ((status & BMC_AUX_MASK) == BMC_AUX_SOP0) receive_packet();
        else trace_add(PDTR_IRQ_NON_SOP, 0, status,
                       (uint32_t)USBPD->BMC_BYTE_CNT);
        USBPD->STATUS = IF_RX_ACT; /* W1C */
    }

}
void phy_init(void) {
    NVIC_DisableIRQ(USBPD_IRQn);
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
    RCC->AHBPCENR |= RCC_USBPD;
    GPIOC->CFGHR &= ~((0xfu << ((14u & 7u) * 4u)) | (0xfu << ((15u & 7u) * 4u)));
    GPIOC->CFGHR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD) << ((14u & 7u) * 4u);
    GPIOC->CFGHR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD) << ((15u & 7u) * 4u);
    AFIO->CTLR |= USBPD_IN_HVT | USBPD_PHY_V33;
    USBPD->DMA = (uint32_t)(uintptr_t)dma_buffer;
    USBPD->CONFIG = IE_RX_ACT | IE_RX_RESET | IE_TX_END | PD_DMA_EN | PD_FILT_EN;
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;
    USBPD->PORT_CC1 &= ~(CC_CMP_MASK | PA_CC_AI);
    USBPD->PORT_CC2 &= ~(CC_CMP_MASK | PA_CC_AI);
    USBPD->PORT_CC1 |= CC_CMP_66;
    USBPD->PORT_CC2 |= CC_CMP_66;
    qhead = qtail = 0; lost=0; attached=false; hard_reset=false; vbus_loss_latched=false;
    hard_reset_tx_active=false; hard_reset_tx_finished=false; hard_reset_tx_timeout=false;
    hard_reset_tx_start_tick=0;
    hr_rx_early=false; hr_early_caps_valid=false;
    selected_cc=0; tx_msg_id=0; tx_wait_crc=false; last_rx_valid=0; retry_count=0; retry_size=0;
    retry_eop_tick=0; crc_wait_armed=false;
    cc_last=false; cc_changed_ms=0;
}
void phy_reset_after_detach(void) { phy_init(); }
void phy_poll_cc(uint32_t now_ms) {
    if (!selected_cc) {
        USBPD->CONFIG &= ~CC_SEL;
        Delay_Us(1);
        bool c1 = (USBPD->PORT_CC1 & PA_CC_AI) != 0;
        USBPD->CONFIG |= CC_SEL;
        Delay_Us(1);
        bool c2 = (USBPD->PORT_CC2 & PA_CC_AI) != 0;
        uint8_t found = c1 ? 1u : (c2 ? 2u : 0u);
        if (found && found != selected_cc) {
            /* Require stable detection; orientation selection is finalized below. */
            if (!cc_last) cc_changed_ms = now_ms;
            cc_last = true;
            if ((uint32_t)(now_ms - cc_changed_ms) >= CC_CONFIRM_MS) {
                selected_cc = found;
                if (found == 1u) USBPD->CONFIG &= ~CC_SEL;
                else USBPD->CONFIG |= CC_SEL;
                rx_mode(); attached=true;
                trace_add(PDTR_CC_ATTACH, found, (uint16_t)USBPD->CONFIG, 0);
                NVIC_EnableIRQ(USBPD_IRQn);
                enqueue(PE_EV_ATTACH, 0);
            }
        } else { cc_last=false; }
    } else {
        bool present = selected_cc == 1u ? (USBPD->PORT_CC1 & PA_CC_AI) != 0
                                             : (USBPD->PORT_CC2 & PA_CC_AI) != 0;
        if (present) { cc_last=true; cc_changed_ms=now_ms; }
        else if (cc_last) { cc_last=false; cc_changed_ms=now_ms; }
        else if ((uint32_t)(now_ms - cc_changed_ms) >= CC_DETACH_MS) {
            attached=false; enqueue(PE_EV_DETACH, 0);
            NVIC_DisableIRQ(USBPD_IRQn);
            trace_add(PDTR_CC_DETACH, selected_cc, 0, lost);
            selected_cc=0;
        }
    }
}
bool phy_pop(pe_event_t *event, pe_caps_t *caps) {
    NVIC_DisableIRQ(USBPD_IRQn);
    if (qtail == qhead) { if (attached) NVIC_EnableIRQ(USBPD_IRQn); return false; }
    volatile rx_event_t *r = &queue[qtail];
    *event = r->type;
    caps->count = r->caps.count; caps->msg_id=r->caps.msg_id;
    for (uint8_t i=0;i<caps->count && i<7;i++) caps->pdo[i]=r->caps.pdo[i];
    qtail = (uint8_t)((qtail+1u)%Q_LEN);
    if (attached) NVIC_EnableIRQ(USBPD_IRQn);
    return true;
}
bool phy_send(pe_cmd_t command, uint32_t rdo, uint32_t now_ms) {
    (void)now_ms; /* Retain API; retry timing now comes from IF_TX_END. */
    /* M13a: check the Hard Reset latch AFTER masking the PD interrupt.
       Checking before the mask creates a race in which Hard Reset arrives
       between the check and the start of a new transmission. */
    NVIC_DisableIRQ(USBPD_IRQn);
    if (!attached || hard_reset || vbus_loss_latched) {
        if (attached) NVIC_EnableIRQ(USBPD_IRQn);
        return false;
    }
    if ((USBPD->CONTROL & PD_TX_EN) || tx_wait_crc) {
        NVIC_EnableIRQ(USBPD_IRQn); return false;
    }
    uint8_t type, count, sz;
    if (command == PE_CMD_REQUEST) { type=PD_DATA_REQUEST; count=1; sz=6; }
    else if (command == PE_CMD_SOFT_RESET) { type=PD_CTRL_SOFT_RESET; count=0; sz=2;
         tx_msg_id=0; last_rx_valid=0; }
    else if (command == PE_CMD_ACCEPT_SOFT_RESET) { type=PD_CTRL_ACCEPT; count=0; sz=2;
         tx_msg_id=0; last_rx_valid=0; }
    else if (command == PE_CMD_GET_SOURCE_CAP) { type=PD_CTRL_GET_SOURCE_CAP; count=0; sz=2; }
    else { NVIC_EnableIRQ(USBPD_IRQn); return false; }
    uint16_t h = hdr(type, count, tx_msg_id);
    memcpy(dma_buffer, &h, 2);
    if (count) memcpy(&dma_buffer[2], &rdo, 4);
    tx_expected_id=tx_msg_id; tx_wait_crc=true;
    crc_wait_armed=false;  /* armed by IF_TX_END, never by TX start */
    memcpy(retry_frame,dma_buffer,sz); retry_size=sz; retry_count=0;
    transmit(sz);
    trace_add(PDTR_TX_COMMAND, (uint8_t)command, h, rdo);
    NVIC_EnableIRQ(USBPD_IRQn);
    return true;
}
bool phy_send_hard_reset_once(void) {
    /* Bench-only API retained for M13a-5/M13a-7 regression tests.
       The M13b normal main.c never calls this function. A controlled caller
       must gate its use and explicitly manage contract invalidation. */
    NVIC_DisableIRQ(USBPD_IRQn);
    if (!attached || hard_reset || vbus_loss_latched ||
        tx_wait_crc || (USBPD->CONTROL & PD_TX_EN)) {
        if (attached) NVIC_EnableIRQ(USBPD_IRQn);
        return false;
    }
    hard_reset = true; /* gate ISR GoodCRC and all future phy_send() calls */
    hr_rx_early = false;
    hr_early_caps_valid = false;
    tx_wait_crc = false;
    crc_wait_armed = false;
    retry_count = 0u;
    retry_size = 0u;
    retry_eop_tick = 0u;
    tx_msg_id = 0u;
    tx_expected_id = 0u;
    last_rx_valid = 0u;
    last_rx_id = 0u;
    qhead = qtail = 0u;
    /* Foreground immediately calls PE_EV_HARD_RESET_TX. Do not enqueue
       the same event again: it would count one reset twice. */
    hard_reset_tx_active = true;
    hard_reset_tx_finished = false;
    hard_reset_tx_timeout = false;
    hard_reset_tx_start_tick = SysTick->CNTL;
    /* RM §21.2.7: the 4 K-code preamble is generated by the USBPD block;
       no ordinary SOP header, CRC or GoodCRC is part of Hard Reset. */
    USBPD->BMC_CLK_CNT = UPD_TMR_TX;
    USBPD->TX_SEL = PD_HARD_RESET_TX_SEL;
    USBPD->BMC_TX_SZ = 0u;
    USBPD->STATUS = IF_TX_END; /* W1C any stale completion */
    USBPD->CONTROL |= BMC_START | PD_TX_EN;
    trace_add(PDTR_TX_HARD_RESET, 0u, PD_HARD_RESET_TX_SEL, 0u);
    NVIC_EnableIRQ(USBPD_IRQn);
    return true;
}
bool phy_hard_reset_tx_finished(void) { return hard_reset_tx_finished; }
bool phy_hard_reset_tx_timed_out(void) { return hard_reset_tx_timeout; }

bool phy_hr_arm_rx(void) {
    NVIC_DisableIRQ(USBPD_IRQn);
    bool ok = attached && hard_reset && !vbus_loss_latched &&
              !hard_reset_tx_active && !hard_reset_tx_timeout &&
              !(USBPD->CONTROL & PD_TX_EN);
    if (ok && !hr_rx_early) {
        hr_rx_early = true;
        hr_early_caps_valid = false;
        rx_mode();
        trace_add(PDTR_HR_RX_ARM, 0u, 0u, 0u);
    }
    if (attached) NVIC_EnableIRQ(USBPD_IRQn);
    return ok;
}

bool phy_hr_complete_recovery(void) {
    NVIC_DisableIRQ(USBPD_IRQn);
    bool ok = attached && hard_reset && hr_rx_early &&
              !vbus_loss_latched && !hard_reset_tx_active &&
              !hard_reset_tx_timeout && !(USBPD->CONTROL & PD_TX_EN);
    if (ok) {
        /* Reset SOP TX counter for the new contract. Retain the early
           received SourceCaps MessageID for duplicate suppression. */
        tx_msg_id = 0u;
        tx_expected_id = 0u;
        tx_wait_crc = false;
        crc_wait_armed = false;
        retry_count = retry_size = 0u;
        retry_eop_tick = 0u;
        last_rx_valid = hr_early_caps_valid ? 1u : 0u;
        last_rx_id = hr_early_caps_valid ? hr_early_caps.msg_id : 0u;
        qhead = qtail = 0u;
        hard_reset = false;
        hr_rx_early = false;
        if (hr_early_caps_valid) enqueue(PE_EV_CAPS, &hr_early_caps);
        trace_add(PDTR_HR_RECOVERY, hr_early_caps_valid ? 1u : 0u,
                  last_rx_id, 0u);
        hr_early_caps_valid = false;
    }
    if (attached) NVIC_EnableIRQ(USBPD_IRQn);
    return ok;
}

void phy_hr_fail_stop(void) {
    NVIC_DisableIRQ(USBPD_IRQn);
    hard_reset = true;
    hr_rx_early = false;
    hr_early_caps_valid = false;
    tx_wait_crc = false;
    crc_wait_armed = false;
    retry_count = retry_size = 0u;
    qhead = qtail = 0u;
    rx_mode();
    trace_add(PDTR_HR_FAIL_STOP, 0u, 0u, 0u);
    if (attached) NVIC_EnableIRQ(USBPD_IRQn);
}

/* M13c: fail-closed if ADC monitoring is unavailable or a non-HR policy
   timeout/error enters PE_RECOVERY. Both guard the TX and GoodCRC paths,
   flush pending events, and require a real physical detach to recover.
   Unlike a real Hard Reset, these never re-arm the HR VBUS observer. */
static bool fail_closed_for_fault(uint8_t trace_kind, uint16_t detail) {
    NVIC_DisableIRQ(USBPD_IRQn);
    bool changed = false;
    if (attached && !hard_reset && !vbus_loss_latched) {
        vbus_loss_latched = true;
        tx_wait_crc = false;
        crc_wait_armed = false;
        retry_count = retry_size = 0u;
        retry_eop_tick = 0u;
        qhead = qtail; /* no stale caps/Accept/PS_RDY */
        rx_mode();
        trace_add(trace_kind, 0u, detail, 0u);
        changed = true;
    }
    if (attached) NVIC_EnableIRQ(USBPD_IRQn);
    return changed;
}
bool phy_abort_for_adc_failure(void) {
    return fail_closed_for_fault(PDTR_ADC_FAIL_STOP, 0u);
}
bool phy_abort_for_policy_failure(uint16_t pe_state) {
    return fail_closed_for_fault(PDTR_PE_FAIL_STOP, pe_state);
}

bool phy_attached(void) { return attached; }
bool phy_hard_reset_seen(void) { return hard_reset; }
bool phy_vbus_loss_latched(void) { return vbus_loss_latched; }
bool phy_abort_for_vbus_loss(uint16_t adc_raw, uint32_t uncal_mv) {
    /* Foreground called only after the debounced low-VBUS indication.
       Prevent ISR GoodCRC and future PE TX while the CC comparator may
       still say attached for up to 150ms. This is NOT a PD Hard Reset. */
    NVIC_DisableIRQ(USBPD_IRQn);
    bool did_abort = false;
    if (!vbus_loss_latched && attached && !hard_reset) {
        vbus_loss_latched = true;
        tx_wait_crc = false;
        crc_wait_armed = false;
        retry_count = 0u;
        retry_size = 0u;
        retry_eop_tick = 0u;
        qhead = qtail; /* stale Accept/PS_RDY cannot revive a contract */
        rx_mode();    /* disable an in-flight TX; do not send a reset */
        trace_add(PDTR_VBUS_ABORT, 0u, adc_raw, uncal_mv);
        did_abort = true;
    }
    if (attached) NVIC_EnableIRQ(USBPD_IRQn);
    return did_abort;
}

uint32_t phy_lost_events(void) { return lost; }

void phy_maintain(uint32_t now_ms) {
    (void)now_ms; /* Retain the existing public API; use hardware cycle timing. */
    if (hard_reset_tx_active &&
        (uint32_t)(SysTick->CNTL - hard_reset_tx_start_tick) >=
            PD_HARD_RESET_TX_TIMEOUT_TICKS) {
        NVIC_DisableIRQ(USBPD_IRQn);
        if (hard_reset_tx_active &&
            (uint32_t)(SysTick->CNTL - hard_reset_tx_start_tick) >=
                PD_HARD_RESET_TX_TIMEOUT_TICKS) {
            hard_reset_tx_active = false;
            hard_reset_tx_timeout = true;
            rx_mode();
            trace_add(PDTR_TX_HARD_RESET_TIMEOUT, 0u, 0u, 0u);
        }
        if (attached) NVIC_EnableIRQ(USBPD_IRQn);
    }
    if (!attached || hard_reset || vbus_loss_latched || !tx_wait_crc || !crc_wait_armed) return;
    if ((uint32_t)(SysTick->CNTL - retry_eop_tick) < CRC_RECEIVE_TICKS) return;
    NVIC_DisableIRQ(USBPD_IRQn);
    if (vbus_loss_latched || !tx_wait_crc || !crc_wait_armed ||
        (uint32_t)(SysTick->CNTL - retry_eop_tick) < CRC_RECEIVE_TICKS) {
        NVIC_EnableIRQ(USBPD_IRQn);
        return;
    }
    if (USBPD->CONTROL & PD_TX_EN) { NVIC_EnableIRQ(USBPD_IRQn); return; }
    crc_wait_armed = false;
    if (retry_count >= 2u) {
        tx_wait_crc=false;
        /* USB PD 3.2 v1.2, 9.1.2.2.1.9: a terminal transmission
           error consumes this MessageID, even without GoodCRC. */
        tx_msg_id = (uint8_t)((tx_msg_id + 1u) & 7u);
        trace_add(PDTR_TX_FAILED, retry_count, 0, 0);
        enqueue(PE_EV_TX_FAILED,0);
    } else {
        retry_count++;
        memcpy(dma_buffer,retry_frame,retry_size);
        transmit(retry_size);
        trace_add(PDTR_TX_RETRY, retry_count, 0,
                  (uint32_t)retry_size);
    }
    NVIC_EnableIRQ(USBPD_IRQn);
}
