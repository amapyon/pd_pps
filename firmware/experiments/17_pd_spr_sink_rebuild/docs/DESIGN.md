# New policy design: USB PD 3.2 SPR PPS Sink

Reference: supplied **USB_PD_R3.2_V1.2_2026_08_19_redline.pdf**
(Release 3.2, Version 1.2, 19 August 2026).

Relevant specification sections:

- §1.7: layered architecture (Type-C port control / PHY / Protocol / PE / DPM)
- §6.1–6.4: message header, IDs, GoodCRC, Source_Capabilities, PPS APDO/RDO
- §7.1, §7.7: Soft Reset and Hard Reset
- §7.9: SPR power negotiation AMS
- §7.31: timers including sender/receiver response, PPS periodic refresh
- §9.1, §9.2.4: protocol/PE separation and Sink state-machine diagram

## Primary transition matrix in new code

Rows are the new simplified PE states. `—` means not a normal transition;
unsupported messages require protocol layer handling, not silent acceptance.

| State | Attach | New Source_Cap | Accept | PS_RDY | Reject/Wait | Hard Reset | Detach |
|---|---|---|---|---|---|---|---|
| DETACHED | WAIT_CAPS | — | — | — | — | RECOVERY | DETACHED |
| WAIT_CAPS | — | WAIT_ACCEPT | — | — | — | RECOVERY | DETACHED |
| WAIT_ACCEPT | — | WAIT_ACCEPT (replace offer) | WAIT_PS_RDY (if sent) | — | READY (prior contract) or WAIT_CAPS | RECOVERY | DETACHED |
| WAIT_PS_RDY | — | WAIT_ACCEPT (superseded) | — | READY | — | RECOVERY | DETACHED |
| READY | — | WAIT_ACCEPT (always) | — | — | — | RECOVERY | DETACHED |
| SOFT_RESET_WAIT | — | — | WAIT_CAPS | — | RECOVERY | RECOVERY | DETACHED |
| RECOVERY | — | — | — | — | — | RECOVERY | DETACHED |

Additional events:

- READY + PPS refresh timer (5s) -> WAIT_ACCEPT with same PPS RDO.
- READY + manual Soft_Reset request -> SOFT_RESET_WAIT -> WAIT_CAPS (after Accept).
- Any valid Soft_Reset received -> WAIT_CAPS after transmitting Accept.
- WAIT_ACCEPT / SOFT_RESET_WAIT + response timeout -> RECOVERY (fail-closed prototype).
- WAIT_PS_RDY + transition timeout -> RECOVERY.
- WAIT_CAPS + capability timeout -> RECOVERY.

## Protocol vs policy ownership

`pd_phy_ch32x035.c`:
- CC orientation selection, attach/detach filtering
- SOP frame validation, GoodCRC, duplicate Message ID handling
- per-epoch TX Message ID and bounded retransmit
- USBPD status flags: W1C and IRQ processing; DMA read/write
- RX packets -> fixed-size queue without debugprintf

`pd_policy.c`:
- evaluate the **current** advertised APDOs and build a 7V/1A PPS RDO
- send 5V/1A Fixed Request if no suitable PPS APDO is available
- Accept / PS_RDY contract confirmation
- PPS refresh, Soft Reset and power-contract invalidation
- fail-closed recovery until detach (temporary design choice)

`main.c`:
- per-loop priority: CC, PHY service, bounded RX events, PE timeouts, TX commands
- intentionally no synchronous logging, OLED or I2C during PD activity

## Follow-on implementation milestones

1. **Bring-up:** cross compile on actual ch32fun SDK. Check memory, IRQ ABI,
   USBPD status bits, DMA layout, GoodCRC timing, CC1/CC2 orientation.
2. **Protocol hardening:** check revision/header/CRC semantics; duplicate IDs,
   retry count/time, Message Discarding, unexpected messages and AMS arbitration.
3. **PE conformance:** complete Soft Reset and Hard Reset transition-to-default;
   Reject/Wait guards and accurate spec timer start/end points.
4. **Interoperability:** first contract, repeated identical Source_Cap, caps
   6->5 PDO, Soft Reset, hard reset, PPS refresh, detach and both CC orientations.
5. **Restore UI/measurement:** INA226, OLED and console on a strictly lower
   priority time budget, asynchronous snapshot logging.
6. **PPS adjustment:** only after stable 7V/1A conformance tests, add adjustable
   voltage/current with APDO validation and independent protection.

## Tests completed vs not completed

| Test | Result |
|---|---|
| C11 PE compilation and tests | PASS |
| Identical PDO (new MessageID) triggers new Request in PE | PASS |
| 5s PPS refresh in PE | PASS |
| Soft Reset path in PE | PASS |
| Hard Reset blocks Requests in PE | PASS |
| Fixed 5V fallback in PE | PASS |
| CH32 PHY host C syntax preflight (stand-in symbols) | PASS |
| Actual RISC-V build / link with ch32fun | NOT RUN |
| CH32 bench Flash / GoodCRC PHY verification | NOT RUN |
| USB-IF specification certification | NOT PERFORMED |
