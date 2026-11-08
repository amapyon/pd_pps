# M11: One-shot Sink-initiated Soft Reset bench test

## Purpose

With the M10 PPS 7.000 V / 1.000 A operating request confirmed on the same
hardware, test just the **Soft Reset recovery** path without changing the
Policy Engine, PHY or debug transport. This is a controlled, experimental
firmware variant; do not leave it as a production image.

## Change

Only `src/main.c` differs from the M9 diagnostic version:

- On first reaching `PE_READY`, arm a 1000 ms delay.
- After the delay and while still attached / ready, call
  `pe_request_soft_reset(&policy, now)` exactly once per MCU boot.
- The existing foreground send path sends `PE_CMD_SOFT_RESET`.
- No printf is added while the charger is connected.
- M9 RAM trace remains unchanged (127 records maximum, oldest preserved).
- The Soft Reset test does **not** directly set 7 V or change the existing
  request APDO/RDO. No output loads or inline USB meters are permitted.

## Expected trace interpretation

- `code=12 detail=2`: Sink attempted to transmit Soft_Reset.
- `code=10`: GoodCRC received for that send (check `value` bit 8).
- `code=7 detail=6 aux=...`: incoming control message, extract message
  type from low 5 header bits (type 3 = Accept).
- `code=15 detail=3 ...`: `PE_EV_ACCEPT`.
- After source sends fresh Source_Capabilities, `code=7 detail=30`
  plus six `code=8` PDO records and `code=12 detail=1` PPS Request should appear.
- Following Accept and PS_RDY, `PE_READY` is state 4.

This one-shot test does **not** independently exercise *unchanged SourceCaps
with a new Message ID while already in Ready*. That is a separate M11 test.

## Procedure

1. USB-C charger disconnected. Stop any running Log, unzip the patch overlay
   at repository root, Build, Flash, start Log, check M8 heartbeat.
2. While Log is running, attach same known PPS charger **directly** for about
   5 seconds (no inline USB power meter and no external load), then detach.
3. Keep unplugged until `PD17 TRACE END`; share the entire trace.

## Guardrails

- On error or Hard Reset, the existing fail-closed behavior remains.
- If the source unexpectedly refuses Soft Reset, do not infer a PHY issue
  without decoding the complete trace.
- If `trace_dropped` is nonzero, shorten the connected time; don't extend
  the ring buffer in this same test.
