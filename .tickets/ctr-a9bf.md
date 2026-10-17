---
id: ctr-a9bf
status: in_progress
deps: []
links: []
created: 2026-10-17T10:41:55Z
type: bug
priority: 1
---
# Hold loaded terrain until a connectivity change releases it

## Acceptance Criteria

Initially detached imported terrain remains static through physics steps and unrelated edits. Cutting its solid bonds releases isolated descendants. GPU connectivity and persistent flags; tree-cut regression still passes.


## Notes

**2026-10-17T10:47:30Z**

RED: loaded island extracted on first step (4 existing tests passed, new test failed). GREEN: per-cell persisted load/bond/release flags, parallel GPU baseline before first brush/physics, candidate eligibility after lost known solid bonds, unknown-neighbor preservation. Fixed import islands, unrelated edits, cut-before-first-tick and checkpoint roundtrip all pass under Vulkan core+sync validation (7/7). Actual scene/tree-cut app tests 2/2 and terrain prop tests 2/2 pass with same validation.
