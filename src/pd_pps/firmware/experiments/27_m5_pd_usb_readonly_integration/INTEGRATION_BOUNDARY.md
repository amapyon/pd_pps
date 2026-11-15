# Experiment 27 — code integration guide (design, not deployed firmware)

## Foreground-only sequencing

**Pseudocode**, not a replacement for either existing `main.c`:

```c
/* CH32X035 target initialization — future, not implemented here */
SystemInit();
/* init PD17, sensor sampling, USBFS, and USB recovery hooks */
m5_session_init(&session);
m5_usb_recovery_nb_init(&recovery);

for (;;) {
    /* Keep this first: bounded PD PHY/PE deadline service. */
    service_pd_phy_and_policy();
    sample_sensor_if_budget_allows();

    /* Only after PD PE has finished a coherent update. */
    m5_usb_health_t health = {recovery.bus_resets,
                               recovery.tx_timeouts, recovery.reconnects};
    (void)m5_bridge_publish_pd17(&session, &pe, &sensor,
                                 sensor_is_fresh, &health);

    /* A future USBFS adapter must map these actions to CH32X035 registers:
       DROP_PENDING/DETACH => reset line state and flush RX ring;
       DETACH => disable port + USB pullup without blocking;
       REINIT => USBFSSetup() from the foreground, never from an IRQ. */
    m5_usb_recovery_action_t action =
            m5_usb_recovery_nb_poll(&recovery, monotonic_ms);
    const bool drop = action == M5_USB_EVENT_DROP_PENDING ||
                      action == M5_USB_EVENT_DETACH;
    if (drop) discard_pre_reset_rx_ring();
    perform_hardware_action_if_any(action);
    m5_usb_io_poll(&session, &usb_io, drop);
    /* When detached for reenumeration, avoid USB IO until REINIT is done. */
}
```

This is an **ordering proposal**; the USB-PD and USBFS peripheral interrupt integration has not been written. Real implementation must handle the RX ring producer/consumer relation and IRQ event-counter synchronization carefully. The `m5_usb_recovery_nb` host module models events, but is not itself the IRQ-safe MCU driver. Do not call `M8F_RecoveryPoll()` unchanged: its 150ms busy wait is inappropriate in a cooperative PD service loop.

## Key corner cases

- At an ACKed USBFS endpoint **submission**, the send queue advances; M8F's EP3 watchdog remains in flight until the corresponding **IN IRQ**. USB reset must empty all stale bytes from both queue and RX ISR ring. Failed endpoint submission must not advance the queue.
- Do not perform USBFS reset or pin reconfiguration in the USBPD IRQ handler.
- PD17 `pe_t` state, sensor flags, and capability array must be updated by a single main-loop owner. Never read a partially updated `pe_t` from either USB ISR.
- Re-enumeration can take longer than 150ms on Windows and may change the COM port; the PC-side reconnect manager must verify `INFO protocol=7` before issuing PD reads.
- Do not reuse experiment24's `Monitor-M8F-Cdc.ps1` for protocol 7: it requires the **protocol=6** firmware identity and a differently constrained command set.
- `SET_PPS`, `OUTPUT_ON` etc. are rejected. A future output-control API requires a transaction ID, completed-state query and explicit policy/ownership checks; no automatic replay after reconnect.
- USB PD PPS still needs periodic refresh and deadline compliance while USB recovery is pending. Measure time budgets on hardware, do not rely on host-only tests.
