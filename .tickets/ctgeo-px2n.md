---
id: ctgeo-px2n
status: in_progress
deps: []
links: [ctgc-tteu]
created: 2026-11-26T19:16:08Z
type: bug
priority: 1
assignee: kmd
tags: [gas, vulkan]
---
# Use physical cell order for explicit gas sweeps

Fused hybrid shaders invoke the explicit stage before assigning the private implicit scan direction. Explicit shared line storage is indexed by physical coordinates, so it must not depend on the implicit pass or direction bank bit.

## Acceptance Criteria

All standalone and fused explicit shader paths traverse line cells in physical order regardless of implicit direction. Compiled shader regression covers generic, fixed-axis and local-stage paths. Existing native transport comparisons pass.


## Notes

**2026-11-26T19:23:32Z**

The compiled X-wide-local shader at 9513f4f3 calls gas_explicit_run before the first store of transport_pass. The explicit shader call-graph regression built and went RED on all 14 variants. All 14 now pass, including generic and fixed-axis fused shaders, the local-stage shaders, and standalone variants. The four existing shader-memory checks and 26 gas numeric and fixture cases passed. Native comparisons and the full suite are pending.

**2026-11-26T19:28:41Z**

Native gas transport checks passed on the refactor for standalone explicit, generic fused, fixed-axis fused, and fixed-axis cached local stages. Each profile passed the explicit-flux comparison, stationary diagonal transport, and the small- and large-step dormant-face conservation cases, for 16 total passes and no skips. The whole-project make test is still queued on b2710a77.

**2026-11-26T19:33:53Z**

The first matrix omitted DC_GAS_FUSED_AXIS. The rerun pinned DC_GAS_FUSED_AXIS=1 and DC_GAS_FUSED_METADATA=1 and again completed all 16 focused cases with zero failures or skips. Separate native admission comparisons confirmed generic versus fixed-axis dispatches with exact snapshots and 94 versus 94 pressure tiles with exact packets when cached local stages were enabled.
