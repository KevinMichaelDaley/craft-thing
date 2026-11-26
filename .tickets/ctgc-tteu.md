---
id: ctgc-tteu
status: in_progress
deps: []
links: []
created: 2026-11-26T16:31:27Z
type: bug
priority: 1
assignee: kmd
parent: ctsl-7ay0
tags: [fire, gas, vulkan]
---
# Isolate long-run chemical inventory gain in the standing pool

The exact standing-pool target on 61cf9a22 stopped at Greek second 142. With initial chemical stock 9161923 Q16, the load retained 8786543 and the GPU audit reported 375384.0625 in gas products, a gain of 4.0625 Q16 above the locked four-unit tolerance. The prior second was exactly at the tolerance. All 36 faces stayed submerged and no exterior or mobile fuel was observed.

## Design

Compare the existing GPU reduction against a completed-queue sum of the gas owners and pending stock. If they agree, isolate long-run product transport or publication from coating reaction. Keep the authored load, real cadence, and four-unit tolerance.

## Acceptance Criteria

Identify the source of the gain and verify the complete standing-pool replay holds the original finite ledger tolerance at normal cadence through exact depletion and the matched oil control.


## Notes

**2026-11-26T16:54:17Z**

A focused 10-tile Vulkan regression reproduced a 15.6875 Q16 undercount in the old float atomic reduction. Compensated per-workgroup sums and a double-precision host sum now match independently read stock exactly, and the standing-pool gate uses those precise totals at the original tolerance. The dedicated audit tests passed 3/3 and gas numerics and fixtures passed 26/26. Direct pool sampling against the original reduction is still running through the original 142-second failure and a transport-only interval. At 70 seconds the original reduction was within 0.014 Q16 of authoritative stock.
