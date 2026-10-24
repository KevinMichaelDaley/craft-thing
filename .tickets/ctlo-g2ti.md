---
id: ctlo-g2ti
status: in_progress
deps: []
links: []
created: 2026-10-24T05:10:15Z
type: task
priority: 1
---
# Record DRM client engine counters with displayed frame diagnostics

## Acceptance Criteria

Read-only diagnostic records per-client DRM GPU engine time without double-counting shared file descriptors; survives inaccessible/exiting processes. Preserve frequency-only default and never change clocks, priorities or other applications.


## Notes

**2026-10-24T05:13:41Z**

Four frequency/DRM tests pass, including shared descriptors/processes, PCI namespaces and disappearing processes. Read-only displayed real-time tree-cut replay remains failing:35.072ms mean /41.968ms worst over500 frames. Sampler records concurrent client time without modifying clocks, priorities or other applications; final acceptance must also run without sampling.
