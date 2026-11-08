# M4-C: Source Capabilities integrity — host validation candidate

Source: copied unchanged M4-B experimental folder (16_oled_encoder_pps_control) before edits.
Use this as an independent experimental branch, not a validated replacement.

## Scope
- Check received PDO count and frame length (BMC_BYTE_CNT assumed to include 2-byte header and 4-byte CRC; must confirm on CH32X035).
- Check basic SPR fixed PDO and PPS APDO sanity, including the previously observed anomalous FIXED 34.6V word.
- Copy only reported PDO words, clear stale trailing entries.
- Main-thread consumers get an IRQ-protected 28-byte Source Capabilities snapshot.
- Preserve existing source behavior for request selection, Power Limited handling and retries; those still need separate remediation.

## Important limitations
- Host tests do not prove hardware BMC_BYTE_CNT semantics or negotiation compatibility. **Do not flash this candidate before validating RX byte-count semantics.**
- This guard intentionally accepts only SPR fixed supplies up to 20V, not EPR fixed PDOs.
- No guarantee of glitch-free power during Source Capabilities renegotiation.
- Original M4-B folder remains untouched.

Run `make test` inside this experimental folder on a host with gcc.
