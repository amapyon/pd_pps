# M43B-5 PPS refresh / USBFS IRQ window snapshot

Baseline: M43B-4 v01; install to a NEW experiment directory. Existing M13b,
M43A, M43B-2, M43B-3 and M43B-4 files remain unchanged.

## Change scope

1. M43B-4 froze the SRAM ring on FIRST Source Hard Reset. M43B-5 records
   first HR without freezing. Freeze occurs on `TX_FAILED` or **second** Source
   Hard Reset (or prior policy/voltage fault/detach). `reserved1` in the
   64-byte header now stores the count of Source Hard Resets observed in this
   attachment; ring magic/version/size/ELF symbol `m43b3_sram` are unchanged.
2. Each newly sent `PE_CMD_REQUEST` receives a monotonically increasing
   per-attach `req_seq` (16-bit). Retry transmissions keep that sequence.
   Flags tag INITIAL (0), AFTER_SOURCE_HR (1), PPS_REFRESH (2). The last tag
   is driven by an observational `PE_READY -> PE_WAIT_ACCEPT` transition.
3. The preexisting USBFS IRQ handler already counts IRQs during CRCReceive
   windows; M43B-5 reads this 32-bit counter at each `IF_TX_END` arm and
   GoodCRC/timeout/abort, recording the delta in the SRAM ring. **No extra
   USBFS IRQ handler work** is added. Counts are not evidence of IRQ duration,
   physical GoodCRC arrival, or causality.
4. Decoder emits `requests_retained` in `summary.json` and new per-request
   events in `events.csv`. `m43b3_sram` raw ABI stays version 1, 1088 bytes.

## New event codes

- 15 `REQ_BEGIN`: flags 0 initial / 1 post-HR / 2 PPS refresh;
  arg0 request sequence | Request header <<16; arg1 running USBFS CRC count.
- 16 `REQ_WINDOW`: flags 1 matched GoodCRC / 2 CRC timeout /
  3 Hard Reset interrupt / 4 superseded SOP; arg0 sequence | retry <<16 |
  expected ID <<24; arg1 USBFS IRQ count during this one receive window.
- 17 `REQ_END`: flags 1 matched GoodCRC / 2 TX_FAILED /
  3 Source HR abort / 4 SOP superseded; arg0 seq | retry <<16;
  arg1 sum of window IRQs for this Request.
- 18 `HR_INDEX`: flags number of Source Hard Resets (clamped at 255),
  arg0 USBPD status, arg1 CONTROL.

The journal retains LAST 64 events. Old entries overwritten by ring wrap
are not recoverable. `lost=0` is not evidence that nothing was overwritten;
use `total-count` and `oldest_events_overwritten`. The decoder distinguishes
`UNKNOWN_OR_OVERWRITTEN` for a Request whose start was overwritten.

**Limitations:** This is post-mortem register/counter logging, not a CC
waveform capture. Additional writes in the USBPD handler may influence
real-time behavior. Cross-build and hardware behavior must be verified on
Windows/WeAct CH32X035F8U6. Test with PC CDC and PD attached simultaneously
only after verifying PC and charger data-lines, VBUS and ground do not
conflict. Request current is not a current measurement.

The installed `Read-M43B5-SRAM.ps1` must use the M43B-5 ELF; stop PD session,
retain MCU power, close `minichlink -T`, enter `DUMP`, and do not reset until
SRAM capture is complete. Output is in a NEW `diagnostics/m43b5_refresh_irq/
capture_.../` directory; no previous raw logs are overwritten.
