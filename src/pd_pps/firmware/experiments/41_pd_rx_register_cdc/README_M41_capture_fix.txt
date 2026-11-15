M41 capture script fix (no firmware changes)
==========================================

Reason:
- Original Capture-M41-Trace.ps1 stopped on PDSTOP found=1 even if active=0.
- After physical PD detach, the previous stop record remains found=1.
- The existing 926 cumulative trace drops are from a previous PD session.

Install:
- Copy Capture-M41-Trace-Fixed.ps1 to:
  firmware\experiments\41_pd_rx_register_cdc\Capture-M41-Trace-Fixed.ps1
- Keep the M41 firmware already flashed. Do not rebuild or reflash.

Run from C:\pj\pd_pps\src\pd_pps:
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\41_pd_rx_register_cdc\Capture-M41-Trace-Fixed.ps1" -Port COM14 -WaitSeconds 45 -IntervalMs 300 -LogPath ".\M41_trace_new.txt"

Before executing:
- Remove PD charger physically and wait until a=0.
- Keep the PC USB CDC cable attached.
- Run the command, then connect PD charger when prompted.
- Do not disconnect PD until trace extraction finishes.

What changed:
- Uses baseline values and waits for fresh active=1 before accepting found=1.
- Does not interpret cumulative dropped counter as current-session loss.
- Captures PDSTOP, PDSTAB, PDERR and PDTRACE after a fresh failure.

Notes:
- The script has undergone static checks, but cannot be run against a physical
  COM14 port here; user environment verification is needed.
- If firmware resets mid-capture, records may be lost.
