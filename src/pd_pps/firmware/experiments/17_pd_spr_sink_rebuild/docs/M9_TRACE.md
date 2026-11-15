# M9: RAM-only USBPD trace (experimental, 2026-11-07)

This patch is solely an observability change to `17_pd_spr_sink_rebuild`.
Policy Engine logic, physical CC configuration, message transmission order,
DMA ownership, and WCH-LinkE / VS Code / xPack settings are **unchanged**.

## Principles

- During an attached session, no `printf` (or OLED, I2C, or other synchronous
  logging) is performed by M9. The IRQ and the foreground may only write fixed
  records to RAM. This is not a zero-overhead trace; it adds bounded CPU work.
- The trace has 128 slots at 12 bytes per slot (1,536 B) and can hold **127**
  records. On overflow, the **earliest** 127 pending records remain; subsequent
  events are omitted and counted in `trace_dropped`.
- `phy_init()` runs on boot AND after detach and resets its existing RX event
  queue and event-loss counter. The M9 trace is deliberately **not** cleared by
  `phy_init()`, so prior session records survive detach.
- Trace printing starts only after **software-confirmed CC detach plus 500 ms**.
  Printing is then synchronous. Keep the charger disconnected until `TRACE END`.
- Trace timestamps are the lower 32 bits of the SysTick cycle counter (at
  48 MHz, wrap occurs every ~89.5 s). They are not milliseconds and may wrap.
  Only compare close neighboring events; ticks alone cannot prove PD timings
  without checking the actual CPU clock.
- The existing M8 `PD17: alive...` prints every 2 seconds while logically
  detached, outside the trace dump.

## Code meanings (`PD17 TRACE`)

Each line is `tick=... code=... detail=... aux=... value=...`.

| Code | Name | detail | aux | value |
|---:|---|---|---|---|
| 1 | CC_ATTACH | CC1=1, CC2=2 | USBPD CONFIG | 0 |
| 2 | CC_DETACH | selected CC | 0 | RX event queue losses *before* `phy_init()` reset |
| 3 | IRQ_RESET | 0 | captured STATUS | 0 |
| 4 | IRQ_TX_END | 0 | captured STATUS | 0 |
| 5 | IRQ_NON_SOP | 0 | STATUS | BMC byte counter |
| 6 | RX_BAD_LENGTH | received bytes (truncated to 8 bits) | 0 | 0 |
| 7 | RX_HEADER | received bytes | raw PD header | 1 if valid Source_Capabilities recognized |
| 8 | RX_PDO | PDO index (1-based) | Source_Capabilities header | raw 32-bit PDO |
| 9 | TX_GOODCRC | received MessageID | transmitted GoodCRC header | 0 |
| 10 | RX_GOODCRC | received MessageID | received GoodCRC header | low byte=expected TX MessageID; bit8=match and TX was awaiting GoodCRC |
| 11 | RX_DUPLICATE | MessageID | header | 0 |
| 12 | TX_COMMAND | `pe_cmd_t` enum | transmitted header | RDO (relevant for Request) |
| 13 | TX_RETRY | retry count | 0 | transmit size |
| 14 | TX_FAILED | final retry count | 0 | 0 |
| 15 | PE_EVENT | `pe_event_t` enum | previous PE state | new PE state |
| 16 | PE_TICK | previous PE state | new PE state | fault counter |

PE states: `0=DETACHED`, `1=WAIT_CAPS`, `2=WAIT_ACCEPT`, `3=WAIT_PS_RDY`,
`4=READY`, `5=SOFT_RESET_WAIT`, `6=RECOVERY`.

PE event enum: `0=ATTACH`, `1=DETACH`, `2=CAPS`, `3=ACCEPT`,
`4=PS_RDY`, `5=REJECT`, `6=WAIT`, `7=SOFT_RESET_RX`,
`8=HARD_RESET_RX`, `9=TX_FAILED`.

PE command enum: `0=NONE`, `1=REQUEST`, `2=SOFT_RESET`,
`3=ACCEPT_SOFT_RESET`.

`TX_GOODCRC` is a record of local hardware transmit initiation, **not** proof
of successful receipt by the source; `IRQ_TX_END` means the local controller
reported transmit completion. A `TX_COMMAND` record likewise does not prove a
contract; only the PD message sequence, PE transition and independently
measured voltage together provide the intended evidence.

## Expected diagnostic path

1. With USB-C unplugged and WCH-LinkE connected: build / flash / log. Confirm
   the M8 heartbeat still prints and no `TRACE BEGIN` appears at boot.
2. In a separate experiment after confirming build and heartbeat, keep Log
   running, connect a known PPS source directly **without an inline meter**,
   then unplug it. Keep it unplugged until `TRACE END` prints.
3. Copy the entire trace, especially `TRACE END trace_dropped=...`.
   If no `TRACE BEGIN` appears, it does **not** prove the MCU or PD PHY failed:
   the firmware may not have confirmed either attach or detach, and this version
   intentionally does not print while attached.

Never connect high-power loads, motors or battery charging hardware for these
bring-up tests. This is diagnostic firmware, not a production PD stack.
