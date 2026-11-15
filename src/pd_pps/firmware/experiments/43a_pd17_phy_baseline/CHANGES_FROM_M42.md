# Changes from M42 to M43A

1. PHY from verified earlier `17_pd_spr_sink_rebuild` / M13b; byte-for-byte identical original up to appended M40 read-only snapshot function.
2. Remove M41 read-only IRQ and timeout trace hooks (code 29..34 no longer emitted). Core timing, protocol & safety remain exactly PD17/M13b.
3. M42 CDC, ADC, INA cache, policy unchanged (except PONG M43A identity).
4. USB product/serial 043 for unambiguous experiment identity; project Makefile target renamed.
5. M42 unit tests adjusted only for intentional PHY restoration/version identity.

**No flashing and no electrical hardware verification was performed here.**

## Build fix (2026-11-15)

- `m43a_pd17_phy_baseline.c` now `#include "src/main.c"`, consistent with M42's TARGET wrapper.
- Fixes `undefined reference to main` during linking. **No PD PHY, policy, ADC, INA, or USB logic changed.**
- Added host regression test to check TARGET wrapper.
