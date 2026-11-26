---
id: ctgeo-epi8
status: open
deps: []
links: [ctsg-6060]
created: 2026-11-26T19:23:42Z
type: bug
priority: 2
assignee: kmd
tags: [gas, vulkan, tests]
---
# Restore lazy pressure clear admission in the publication fixture

The 9513f4f3 Vulkan publication suite stops the lazy-clear comparison at gas_multigrid_cases.h line 77. The fixture sees a different backend marker than requested. Establish which pressure path was admitted and whether the configuration or backend routing owns the discrepancy.

## Acceptance Criteria

Restore the existing lazy and eager admission checks and the exact active, quiet, and dormant stock comparison. Retain pressure correctness, dispatch and capability gates.
