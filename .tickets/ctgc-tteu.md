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

**2026-11-26T17:09:20Z**

Authoritative c6579509 checkpoint sweep completed 150 normal seconds and 12 additional gas-only seconds. At second 142, the physical chemical ledger was +4.028182030 Q16 with 36/36 wet faces and no pending sealed gas. The same run’s old GPU audit differed by only +0.003067970 Q16. From seconds 138 to 150 total drift increased by 0.927549362 Q16, then by only 0.047665596 Q16 over 12 gas-only seconds with fixed fuel stock. The precision repair is independent of the remaining physical gain. A staged probe based on 56c48c9d captured all 60 step boundaries in its one-second preflight and reproduced the normal first-second total +0.000722803 Q16. The longer staged capture is measuring the physical and gas phases separately around the original failure interval. The native five-second Greek and oil controls both completed 300 steps with 36/36 submerged and matched initial fuel and bound water. The four-unit gate remains unchanged.

**2026-11-26T17:29:30Z**

The completed phase probe based on 56c48c9d captured 900 ticks at normal 60 Hz through seconds 131–145 and exited 0. The coating, fluid and thermal tick balanced to exactly 0 Q16 in each second, while the gas step added 1.190847397 Q16 in total. The staged replay crossed the four-unit gate at second 144 and ended at +4.123509407 Q16. A follow-up diagnostic reads the gas owners immediately after the geometry commit dispatch to distinguish geometry remapping and bubble displacement from later gas transport and publication.

**2026-11-26T17:30:52Z**

The geometry-boundary probe passed its 60-tick preflight. Its +0.000722803 Q16 ledger total matched the prior staged and native first second. The measured split was +0.000006199 from physical preparation, +0.000842408 through gas geometry commit and -0.000125804 afterward. The long probe is running through the prior failure interval for attribution at established pool conditions.

**2026-11-26T17:41:04Z**

The completed geometry-boundary replay measured 900 normal ticks at seconds 131–145. Physical preparation and geometry commit each contributed exactly 0 Q16 across that interval. The following gas transport stages added 1.275294304 Q16 and crossed four at second 141. A separate closed Vulkan regression established an independent contraction defect, gaining 14 Q16 over 200 water cycles in 512 sealed channels without stepping transport. Splitting each shrinking donor’s represented debit among receivers reduced that drift to 1.5625 Q16 with both local and global geometry. All seven focused Vulkan cases and 26 gas numeric and fixture cases passed the refactor. The late pool transport gain remains open.

**2026-11-26T17:43:29Z**

The production transport update separately reproduced one float bit of stock creation on a balanced exchange and lost a 2^-30 incoming trace when old carry was fully sent. Compensating both face additions preserves exact exhaust, soot and pigment in the Vulkan regression. Eight focused transport and display checks and 26 gas numerics and fixture checks passed. The authored full locked depletion replay is required to verify the actual standing-pool ledger through depletion.

**2026-11-26T17:57:18Z**

The exact integrated c4324106 run of make test_greek_siege_locked_depletion exited 2 at Greek second 143. Inventory was +4.030869484 Q16 with all 36 faces still wet, no transferred or mobile film, and 8580 accepted steps. The preceding second was +3.937343597 Q16. The four-unit gate remains in place. A temporary native probe sampling provisional inventory after advection, after pressure finishing, and after publication passed its aligned 60-tick preflight, matching the official first-second total +0.000814356. Its 130-second warm-up and 15-second phase capture are running on the integrated revision.
