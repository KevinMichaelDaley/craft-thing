---
id: ctowpe-l2cj
status: in_progress
deps: []
links: []
created: 2026-11-27T14:32:24Z
type: task
priority: 2
assignee: kmd
parent: ctowpe-t43r
tags: [offscreen, performance, vulkan]
---
# Measure presented frame costs with a saturated offscreen camera cache

## Design

Measure the production camera path after a two-screen eviction at half-native 960x540 and native 1920x1080. Separate setup, admission and warmup from steady presented-frame samples. Validate actual offscreen owner count and elapsed time, capture mean, p95 and worst frame, and run the existing native performance gate on the same hidden display without concurrent test GPU loads.

## Acceptance Criteria

Record both measurements, GPU/viewport/slot counts, sample counts and presentation settings. Keep any missed native frame budget open with a reproducible measurement.
