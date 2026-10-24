---
id: ctlo-6a47
status: in_progress
deps: []
links: []
created: 2026-10-24T05:13:57Z
type: task
priority: 1
---
# Declare GPU body-root access readonly and nonaliasing for consumers

## Acceptance Criteria

Consumer SPIR-V declares the addressed body root readonly and restricted; only the explicit body-index writer retains write access. Preserve dynamic growth, descriptor publication and all GPU results; measure displayed performance before attributing a benefit.

