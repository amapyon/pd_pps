/* M8 host simulation: responses, CDC control and USB endpoint backpressure. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#define M8_HOST_TEST
#include "../m8e_usb_cdc_irq_trace.c"


/* This mock is compiled ONLY into the host test. It is never in firmware. */
static M8_MeasurementStatus mock_status = M8_MEASURE_IO_ERROR;
static uint32_t mock_voltage;
static int32_t mock_current;
void M8_ReadMeasurement(M8_Measurement *out)
{
    out->status = mock_status;
    out->voltage_mV = mock_voltage;
    out->current_mA = mock_current;
}
static struct _USBState ctx;
static uint8_t ep3_dma[64];
static uint8_t response_data[1024];
static unsigned response_used;
static unsigned tx_packets;
static unsigned max_tx_len;
static int busy_once;
static int reject_once;

uint8_t *USBFS_GetEPBufferIfAvailable(int endpoint)
{
    if (endpoint != CDC_IN_EP) abort();
    if (busy_once) { busy_once = 0; return NULL; }
    return ep3_dma;
}
int USBFS_SendEndpoint(int endpoint, int len)
{
    if (endpoint != CDC_IN_EP || len < 1 || len >= 64) abort();
    if (reject_once) { reject_once = 0; return -1; }
    if (response_used + (unsigned)len > sizeof(response_data)) abort();
    memcpy(response_data + response_used, ep3_dma, (size_t)len);
    response_used += (unsigned)len;
    ++tx_packets;
    if ((unsigned)len > max_tx_len) max_tx_len = (unsigned)len;
    HandleInRequest(&ctx, CDC_IN_EP, ep3_dma, len);
    return 0;
}
static void need(int condition, const char *description)
{
    if (!condition) { fprintf(stderr, "FAIL: %s\n", description); exit(1); }
}
static void feed(const uint8_t *data, unsigned n, unsigned chunk)
{
    while (n) {
        unsigned count = n < chunk ? n : chunk;
        if (count > 64) count = 64;
        HandleDataOut(&ctx, CDC_OUT_EP, (uint8_t *)data, (int)count);
        data += count;
        n -= count;
    }
}
static void drain(void)
{
    for (unsigned i = 0; i < 6000; ++i) {
        if (rx_read_idx == rx_write_idx && reply_sent == reply_length) break;
        M8_ServiceUsbCommands();
    }
    need(rx_read_idx == rx_write_idx, "rx ring must drain");
    need(reply_sent == reply_length, "reply must be accepted");
}
static void run_raw(const uint8_t *command_data, unsigned n, unsigned chunk)
{
    response_used = tx_packets = max_tx_len = 0;
    feed(command_data, n, chunk);
    drain();
    need(response_used > 0, "response nonempty");
    need(response_data[response_used-1] == '\n', "response LF termination");
    need(max_tx_len <= 63, "all IN packets are short");
}
static void run_command(const char *cmd, const char *expected)
{
    uint8_t frame[200];
    unsigned n = (unsigned)strlen(cmd);
    need(n + 1 < sizeof(frame), "command fits test frame");
    memcpy(frame, cmd, n); frame[n++] = '\n';
    run_raw(frame, n, 2); /* deliberately split OUT packets */
    need(response_used == strlen(expected), "exact response length");
    need(memcmp(response_data, expected, response_used) == 0, "exact response data");
}
int main(void)
{
    need(sizeof(reply) == 192, "bounded response memory");
    run_command("INFO", "INFO board=CH32X035F8U6 firmware=m8e_usb_cdc_irq_trace protocol=5\n");
    run_command("MEASURE", "MEASURE status=error reason=sensor_io voltage_mV=NA current_mA=NA\n");
    mock_status = M8_MEASURE_OK;
    mock_voltage = 12000;
    mock_current = -725;
    run_command("MEASURE", "MEASURE status=ok voltage_mV=12000 current_mA=-725\n");
    mock_current = 0;
    run_command("MEASURE", "MEASURE status=ok voltage_mV=12000 current_mA=0\n");
    mock_current = INT32_MIN;
    run_command("MEASURE", "MEASURE status=ok voltage_mV=12000 current_mA=-2147483648\n");
    mock_status = M8_MEASURE_IO_ERROR;
    run_command("MEASURE", "MEASURE status=error reason=sensor_io voltage_mV=NA current_mA=NA\n");
    mock_status = (M8_MeasurementStatus)99;
    run_command("MEASURE", "MEASURE status=error reason=backend_invalid voltage_mV=NA current_mA=NA\n");
    mock_status = M8_MEASURE_IO_ERROR;
    run_command("PING", "PONG\n");
    run_command("HELP", "HELP INFO STATUS MEASURE PING HELP\n");
    run_command("BOGUS", "ERR unknown_command\n");
    run_command("", "ERR unknown_command\n");
    run_command("THIS_LINE_IS_WAY_TOO_LONG_FOR_THE_COMMAND_PROTOCOL", "ERR line_too_long\n");
    {
        const uint8_t invalid[] = {'I', 0x01, 'F', 'O', '\n'};
        run_raw(invalid, sizeof(invalid), 1);
        need(response_used == strlen("ERR invalid_character\n") &&
             memcmp(response_data, "ERR invalid_character\n", response_used) == 0,
             "reject non-ASCII control bytes");
    }
    {
        const uint8_t crlf[] = {'I','N','F','O','\r','\n'};
        run_raw(crlf, sizeof(crlf), 3);
        need(response_used > 5 && !memcmp(response_data,"INFO ",5), "accept CRLF");
    }
    {
        static const uint8_t status[] = {'S','T','A','T','U','S','\n'};
        busy_once = 1;
        reject_once = 1;
        run_raw(status, sizeof(status), 1);
        /* In this host model, the first submit fails; foreground retries. */
        need(response_used > 63, "STATUS crosses USB packet boundary");
        need(!memcmp(response_data,"STATUS rx_packets=",18), "status prefix");
        int has_overflows = 0;
        for (unsigned i = 0; i + 13 < response_used; ++i)
            if (!memcmp(response_data + i, "rx_overflows=", 13)) has_overflows = 1;
        need(has_overflows, "status includes RX overflow count");
        need(tx_packets >= 2, "STATUS fragmented into multiple USB IN packets");
        need(m8_usb_diag.tx_rejected == 1u, "TX failure recorded and retried");
        need(m8_usb_diag.ep3_busy_polls == 1u, "busy poll recorded");
    }
    /* CDC requests follow pinned fsusb.c handler's STALL/ZLP convention. */
    ctx.USBFS_SetupReqType = CDC_CLASS_INTERFACE_IN;
    ctx.USBFS_SetupReqLen = 7;
    need(HandleSetupCustom(&ctx, CDC_GET_LINE_CODING) == 7, "GET_LINE_CODING");
    need(ctx.pCtrlPayloadPtr == cdc_line_coding, "coding reply pointer");
    ctx.USBFS_SetupReqType = CDC_CLASS_INTERFACE_OUT;
    ctx.USBFS_SetupReqLen = 0;
    need(HandleSetupCustom(&ctx, CDC_SET_CONTROL_LINE_STATE) < 0, "CDC control ZLP ACK");
    need(ctx.pCtrlPayloadPtr == NULL, "clear stale EP0 payload");
    need(HandleSetupCustom(&ctx, 0x7f) == 0, "unknown CDC request STALL");
    ctx.USBFS_SetupReqLen = 7;
    need(HandleSetupCustom(&ctx, CDC_SET_LINE_CODING) == 7, "SET_LINE_CODING");
    uint8_t coding[7] = {0x80, 0x25, 0, 0, 0, 0, 8};
    HandleDataOut(&ctx, 0, coding, 7);
    need(!memcmp(cdc_line_coding,coding,7), "SET_LINE_CODING payload");
    need(rx_overflows == 0, "no RX overflow in ordinary command test");
    need(m8_usb_diag.tx_accepted == m8_usb_diag.ep3_in_completed, "TX accept / completion balanced");
    puts("PASS: M8 commands / MEASURE states / framing / short IN / busy retry / CDC controls");
    return 0;
}
