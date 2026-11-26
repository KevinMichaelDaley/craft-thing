---
id: ct-z4da
status: closed
deps: []
links: []
created: 2026-11-26T05:42:31Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, beaver]
---
# Make live harvesting validate player reach and share material inventory mapping

The playtest enabled stone twice through duplicate wood-kind mappings. World chewing currently checks distance from aim to the prospective cell, without validating the distance from the responsible beaver.

## Acceptance Criteria

Use one physical material eligibility and inventory mapping. Test owned stone and wood credits, player reach, release and retry semantics, and consistent work across action cadence.


## Notes

**2026-11-26T05:50:21Z**

Current world adapter scans every GPU slot without comparing the downloaded chunk coordinate to the aim. It can price a different chunk and then paint the aim chunk before crediting inventory. Match by active world page, preflight the same cell, and add a decoy-chunk regression in addition to distance and cadence.

**2026-11-26T06:08:28Z**

RED reproduced a distant cut through live C input, inventory credit from a decoy chunk, a remapped stone cut at the wrong address, and overflow that removed the source before credit. The live adapter now validates body reach, resolves the active world page, prices stored dry parcel mass, and credits the terrain cut result once. The same eligible target drives animation. Final checks passed 7 work, 9 Vulkan inventory, 1 live app, 8 ability, 2 build recipe, and 1 brush-region test. Coverage includes a partial parcel across a chunk seam, competing players, release, target invalidation, cadence, full-inventory retry, and real structural detachment. The full suite and standalone rigid carve checks are in progress.

**2026-11-26T06:10:13Z**

Post-push regression checks also passed all 12 rigid carve cases, 3 player runtime cases, and 3 split-screen cases. The first full suite was still running its long forest bonfire coverage, with the continue-through-failures rerun queued behind it.
