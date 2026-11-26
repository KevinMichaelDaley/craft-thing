---
id: ct-z4da
status: in_progress
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
