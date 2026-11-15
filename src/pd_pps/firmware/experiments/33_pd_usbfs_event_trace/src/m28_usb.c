/* M28: hardware-specific USBFS adapter; PD17 Policy Engine is not modified.
 * No blocking wait for USB reconnection and no PD/INA calls in USB ISR.
 * Test-only VID/PID and descriptors: not product authorization. */
#include "m28_usb.h"
#include "m30_diag.h"
#include "m32_diag.h"
#include "m28_runtime.h"
#include "ch32fun.h"
#include "fsusb.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CDC_OUT_EP 2
#define CDC_IN_EP  3
#define CDC_SET_CONTROL_LINE_STATE    0x22u
#define CDC_CLASS_INTERFACE_OUT       0x21u
#define CDC_CLASS_INTERFACE_IN        0xA1u

static m28_runtime_t runtime;
/* Set once each foreground iteration. Avoid 64-bit division per USB packet. */
static uint32_t current_service_ms;
static uint8_t cdc_line_coding[7] = {
    0x00,0xC2,0x01,0x00, 0x00,0x00,0x08
};
static volatile uint8_t expecting_line_coding;

/* The local m28_fsusb.c calls this for real bus RESET, independently of
 * EP3 completion. IRQ only increments an event counter. */
void m28_usb_on_bus_reset_irq(void) {
    m30_diag_bus_reset_irq();
    m32_diag_bus_reset_irq();
    m28_transport_bus_reset_irq(&runtime.transport);
}

void HandleDataOut(struct _USBState *ctx, int endpoint, uint8_t *data, int len) {
    if (endpoint == 0) {
        if (expecting_line_coding && len == (int)sizeof(cdc_line_coding)) {
            for (size_t i=0; i<sizeof(cdc_line_coding);++i)
                cdc_line_coding[i] = data[i];
        }
        expecting_line_coding = 0;
        ctx->USBFS_SetupReqLen = 0;
    } else if (endpoint == CDC_OUT_EP && len >= 0 && len <= 64) {
        m28_transport_rx_irq(&runtime.transport, data, (size_t)len);
    }
}

int HandleInRequest(struct _USBState *ctx, int endpoint, uint8_t *data, int len) {
    (void)ctx; (void)data; (void)len;
    if (endpoint == CDC_IN_EP)
        m28_transport_in_complete_irq(&runtime.transport);
    return 0;
}

/* fsusb.c semantics: >0 data stage, 0 STALL, <0 status ACK. */
int HandleSetupCustom(struct _USBState *ctx, int setup_code) {
    ctx->pCtrlPayloadPtr = NULL;
    expecting_line_coding = 0;
    switch ((unsigned)setup_code) {
    case CDC_SET_LINE_CODING:
        if (ctx->USBFS_SetupReqType != CDC_CLASS_INTERFACE_OUT ||
            ctx->USBFS_SetupReqLen != sizeof(cdc_line_coding)) return 0;
        expecting_line_coding = 1;
        return (int)sizeof(cdc_line_coding);
    case CDC_GET_LINE_CODING:
        if (ctx->USBFS_SetupReqType != CDC_CLASS_INTERFACE_IN ||
            ctx->USBFS_SetupReqLen == 0) return 0;
        ctx->pCtrlPayloadPtr = cdc_line_coding;
        return ctx->USBFS_SetupReqLen < sizeof(cdc_line_coding)
             ? (int)ctx->USBFS_SetupReqLen : (int)sizeof(cdc_line_coding);
    case CDC_SET_CONTROL_LINE_STATE:
    case CDC_SEND_BREAK:
        if (ctx->USBFS_SetupReqType != CDC_CLASS_INTERFACE_OUT ||
            ctx->USBFS_SetupReqLen != 0) return 0;
        return -1;
    default:
        return 0;
    }
}

static bool rx_pop(void *ctx, uint8_t *byte) {
    m28_runtime_t *r = (m28_runtime_t*)ctx;
    return m28_transport_pop(&r->transport, byte);
}
static int tx_submit(void *ctx, const uint8_t *bytes, size_t len) {
    m28_runtime_t *r = (m28_runtime_t*)ctx;
    if (!bytes || len == 0 || len > 63 || !USBFSCTX.USBFS_DevConfig)
        return -1;
    uint8_t *buffer = USBFS_GetEPBufferIfAvailable(CDC_IN_EP);
    if (!buffer) return -1;
    memcpy(buffer, bytes, len);
    /* Must arm watchdog BEFORE enabling EP3 ACK. */
    m5_usb_recovery_nb_submit_begin(&r->recovery, current_service_ms);
    int rc = USBFS_SendEndpoint(CDC_IN_EP, (int)len);
    m5_usb_recovery_nb_submit_result(&r->recovery, rc==0);
    return rc;
}

/* CH32X035 USB-D+ pullup can be controlled by AFIO CTLR.
 * This detach path changes ONLY USBFS and D+/- pullup. Do not reset RCC,
 * GPIOC (especially PC14/PC15), USBPD, I2C, ADC, or PE state. */
static void usb_detach(void) {
    NVIC_DisableIRQ(USB_IRQn);
    USBFS->INT_EN = 0;
    USBFS->UDEV_CTRL = 0;
    USBFS->BASE_CTRL = 0;
    AFIO->CTLR &= ~UDP_PUE_MASK;
}
static void usb_attach(void) {
    /* USBFSSetup includes a nominal 1ms PHY/PVD delay on CH32X035.
     * It is NOT the previous 150ms Delay_Ms(150) busy wait. The ~1ms
     * critical-section impact must still be checked on hardware. */
    (void)USBFSSetup();
}
void m28_usb_init(void) {
    m28_runtime_init(&runtime);
    expecting_line_coding = 0;
    (void)USBFSSetup();
}
void m28_usb_service(uint32_t now_ms, const pe_t *pe,
                     const pd_sensor_reading_t *reading, bool reading_fresh) {
    current_service_ms = now_ms;
    const m5_usb_io_t io = {rx_pop,tx_submit,&runtime};
    const bool configured = USBFSCTX.USBFS_DevConfig != 0;
    m28_action_t action = m28_runtime_poll(&runtime, now_ms, pe,
                                          reading, reading_fresh, &io, configured);
    if (action == M28_STOP_USB) usb_detach();
    else if (action == M28_START_USB) usb_attach();
}
