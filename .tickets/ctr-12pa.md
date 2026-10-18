---
id: ctr-12pa
status: in_progress
deps: []
links: []
created: 2026-10-18T02:32:21Z
type: bug
priority: 1
---
# Exclude loose particles from terrain component extraction and restoration

## Acceptance Criteria

Tagged terrain grains join a detached mixed body; untagged dirt and leaf debris touching it remain particles, do not bridge terrain components, and remain untagged across save/load and prop bond restoration.

