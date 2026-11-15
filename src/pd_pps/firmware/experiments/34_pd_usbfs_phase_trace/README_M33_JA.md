# M33 USBFS event trace (M32-derived)

Changes: IRQ token counters (SETUP/IN/OUT), RESET/SUSPEND counters, bounded event samples, SET_CONFIGURATION events. No PD Policy Engine or USB recovery changes. M29 LED + M30 DIAG + M32 XDIAG retained. Never overwrite M32.

Placement: firmware/experiments/33_pd_usbfs_event_trace

Commands (PowerShell from repository root):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\33_pd_usbfs_event_trace\Test-M33-Host.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\33_pd_usbfs_event_trace\Build-M33.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\33_pd_usbfs_event_trace"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\33_pd_usbfs_event_trace\Read-M33-Diag.ps1" -Port COM10
```

Only flash after host static tests, cross-build, and restoring a safe test power configuration. Preserve M32 firmware binary as recovery. For diagnosis: connect PC CDC and verify basic commands, then attach PD, confirm failure and LED heartbeat, detach PD, wait for COM to recover, read TDIAG. The trace is RAM-only; MCU reset loses evidence.

TDIAG = number of events, PD attached state, captured session marker; TDIAG C = PD-connection-period token counts (in/out/setup/reset/suspend); TDIAG B = cumulative counters at PD attach; TDIAG P = PD attach and detach mainloop timestamps, USB configuration and UDEV_CTRL at transitions. TDIAG n = up to 40 representative IRQ events: t (approximate ms; foreground-updated), ev (I IN/O OUT/S SETUP/R reset/P suspend/C configuration), ep, fg (raw interrupt flag/status word), c (USB configuration), u (UDEV_CTRL). Per-class quotas intentionally bound log growth; overall counters are not capped. Polling/event activity after initial capture is reflected by counts, but an ISR-level event may be dropped if record capacity or quota is exhausted.

**Important limitations:** USB IN/OUT counts reflect transactions observed by the USBFS ISR, not necessarily host intent when no IRQ fires. Storing last foreground time does not provide precise ISR timestamps. `TDIAG C` counts the interval from the first PD attach until PD detaches; it excludes pre-attach USB activity. A PD connection that fails to reach `phy_attached()` may not trigger trace. `c=01` means firmware configuration state, not proof of an operating PC link. High-speed electrical effects need proper USB instrumentation. Cross-build has NOT been performed in this delivery environment.
