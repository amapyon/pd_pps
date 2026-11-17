# M43B-8 A: pure power-plan extraction

This is a new, additive experimental firmware. Its verified source baseline is
M43B-7 P2 at Git commit `4e8939fcca5004e28042c69e591cb76ddf248725`.
The original experiment remains unchanged.

## Intent

Extract Source PDO / SPR PPS APDO selection and RDO construction out of the
PD policy engine to `src/pd_power.c/.h`. The policy engine still controls all
requests, transition timers, retries, contract validity and safety stop paths.
This stage does **not** enable user-settable voltage/current at runtime.

`pd_power_select()` is deterministic and uses no CH32 registers, interrupts,
USB operations, I2C operations, allocations, prints, or global state.

## Preserved runtime behavior

- PPS target remains 7,000 mV / 1,000 mA.
- Prefer first SPR PPS APDO that covers that target.
- Otherwise, fixed 5,000 mV / 1,000 mA if PDO 1 advertises at least 1 A.
- PE state transitions, timeouts, retry counts, PHY, USBFS, CDC and SRAM ABI
  remain unchanged.
- Hard Reset and ADC/VBUS failure behavior remains unchanged.

## Important distinction

`pd_power_select()` returns a **candidate RDO**; it does not establish a power
contract. The optional variable target argument prepares for future work; it
is not exposed as a UI/CDC APPLY command. In particular, fixed-5V fallback
remains fixed at 1 A independent of its target parameters, matching P2.

## Tests

Run from this experiment's directory:

```sh
python -m unittest discover -s tests -p 'test_*.py' -v
```

Important: the copied M43B-7 tests include source fingerprint guards. The
installer updates ONLY the intentional policy/target/wrapper exemptions; it
continues to protect the PHY, USBFS ISR, CDC, VBUS, and SRAM files. New tests
compare the P2 policy with the new policy through 32 deterministic seeds and
320,000 randomized steps plus directed sequences.

Cross-compile and actual hardware operation must be verified on the user's
Windows development environment. Do not flash without confirming firmware
identity, intended cable/CC and no external load.
