---
id: ct-z4da
status: open
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

