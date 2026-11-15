# M43B-7 P2 session refactor

Project: `43b7_p2_session_refactor`, target `m43b7_p2_session_refactor`.
Previous M43B-6 P0/P1 remains untouched. This is a separate experiment.

- `src/main.c`: hardware bring-up and IRQ order, LED tick, `pd_session_step`.
- `src/pd_session.c`: fixed seven-stage safety->PE->sensors->CDC sequence.
- `src/pd_session_diag.c`: output only after physical detach+500ms.
- Original M43B-6 `main.c` pinned in `tests/fixtures` (not linked).
- USBPD ISR, USBFS ISR, M43B-6 P1 policy/USB command parser not changed.
- No intentional behavioral changes. P2 introduces a separate ELF layout and
  thus real board tests remain necessary. See ZIP `tests/TEST_PLAN.md`.
