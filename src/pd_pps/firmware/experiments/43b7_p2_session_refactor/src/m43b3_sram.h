#ifndef M43B3_SRAM_H
#define M43B3_SRAM_H
#include <stdint.h>

/* One 16-byte record and one fixed 64-byte header; raw little-endian RV32.
 * Symbols are retained in the ELF for post-mortem SRAM extraction. */
#define M43B3_SRAM_MAGIC 0x33425350u /* memory bytes: 'PSB3' */
#define M43B3_SRAM_END 0xA53CB35Au
#define M43B3_SRAM_VERSION 1u
#define M43B3_SRAM_HEADER_BYTES 64u
#define M43B3_SRAM_RECORD_BYTES 16u
#define M43B3_SRAM_CAPACITY 64u

enum {
    M43B3_EVT_TRACE = 1u,       /* flags=PDTR kind; arg0=detail|aux<<8, arg1=trace value */
    M43B3_EVT_TX_END = 2u,      /* arg0=IRQ STATUS, arg1=CONTROL before RX */
    M43B3_EVT_RX_ARM = 3u,      /* arg0=IRQ STATUS, arg1=CONTROL after RX */
    M43B3_EVT_RX_HEADER = 4u,   /* arg0=header|bytecount<<16, arg1=STATUS|CONTROL<<16 */
    M43B3_EVT_GOODCRC = 5u,     /* flags=match|wait<<1|armed<<2, arg0=header, arg1=expected|retry<<8 */
    M43B3_EVT_TX_GOODCRC = 6u,  /* arg0=STATUS, arg1=CONTROL after TX */
    M43B3_EVT_RETRY_STATE = 7u, /* arg0=STATUS, arg1=CONTROL|retry<<16 */
    M43B3_EVT_HR_STATE = 8u,    /* arg0=IRQ STATUS, arg1=CONTROL */
    M43B3_EVT_FAULT_STATE = 9u, /* arg0=STATUS, arg1=CONTROL */
    M43B4_EVT_TX_LAUNCH = 10u, /* arg0=header|n<<16|TX_SEL<<24, arg1=STATUS|CONTROL<<16 */
    M43B4_EVT_RX_IRQ = 11u, /* arg0=STATUS|bytecnt<<16|CC<<24, arg1=header|CONTROL<<16 */
    M43B4_EVT_RX_SAMPLE = 12u, /* arg0=DMA[0:4], arg1=DMA[4:6]|CONFIG<<16|retry<<24 */
    M43B4_EVT_RX_ARM_REG = 13u, /* arg0=CONTROL|CONFIG<<8|RXCLK<<16|CC<<24, arg1=STATUS|expected<<16|retry<<24 */
    M43B4_EVT_CRC_TIMEOUT = 14u, /* arg0=STATUS|bytecnt<<16|CC<<24, arg1=CONTROL|retry<<16|expected<<24 */
    M43B5_EVT_REQ_BEGIN = 15u, /* flags=phase(0 initial,1 HR recovery,2 refresh); arg0=seq|header<<16, arg1=crc_count */
    M43B5_EVT_REQ_WINDOW = 16u, /* flags=1 GoodCRC match,2 timeout; arg0=seq|retry<<16|expected<<24, arg1=usbfs IRQ delta */
    M43B5_EVT_REQ_END = 17u, /* flags=1 GoodCRC match,2 TX_FAILED,3 HR abort,4 other; arg0=seq|retry<<16, arg1=sum of CRC window IRQ */
    M43B5_EVT_HR_INDEX = 18u /* flags=count capped to 255; arg0=STATUS; arg1=CONTROL */
};
enum {
    M43B3_FREEZE_NONE = 0u,
    M43B3_FREEZE_TX_FAILED = 1u,
    M43B3_FREEZE_SOURCE_HR = 2u,
    M43B3_FREEZE_POLICY_FAIL = 3u,
    M43B3_FREEZE_DETACH = 4u,
    M43B3_FREEZE_VBUS_FAULT = 5u
};

typedef struct {
    uint32_t tick;
    uint32_t arg0;
    uint32_t arg1;
    uint16_t seq;
    uint8_t event;
    uint8_t flags;
} m43b3_sram_entry_t;

typedef struct {
    uint32_t magic;             /* word 0 */
    uint32_t version;           /* word 1 */
    uint32_t header_bytes;      /* word 2 */
    uint32_t record_bytes;      /* word 3 */
    uint32_t capacity;          /* word 4 */
    uint32_t session;           /* word 5 */
    uint32_t head;              /* word 6: next write slot */
    uint32_t count;             /* word 7: retained records <=64 */
    uint32_t total;             /* word 8: total records written */
    uint32_t freeze_reason;     /* word 9 */
    uint32_t freeze_tick;       /* word 10 */
    uint32_t active;            /* word 11 */
    uint32_t lost;              /* word 12: reserved for future ring errors */
    uint32_t reserved0;         /* word 13 */
    uint32_t reserved1;         /* word 14 */
    uint32_t end_magic;         /* word 15 */
    m43b3_sram_entry_t records[M43B3_SRAM_CAPACITY];
} m43b3_sram_t;

extern volatile m43b3_sram_t m43b3_sram;
void m43b3_sram_attach(uint32_t tick);
void m43b3_sram_note(uint8_t event, uint8_t flags,
                     uint32_t arg0, uint32_t arg1, uint32_t tick);
void m43b3_sram_trace(uint8_t pdtr_kind, uint8_t detail,
                      uint16_t aux, uint32_t value, uint32_t tick);
void m43b3_sram_freeze(uint32_t reason, uint32_t tick);
void m43b3_sram_detach(uint32_t tick);
#endif
