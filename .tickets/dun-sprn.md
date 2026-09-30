---
id: dun-sprn
status: in_progress
deps: []
links: []
created: 2026-09-30T03:42:15Z
type: task
priority: 0
assignee: kmd
parent: dun-v2vu
tags: [gpu, performance, ui]
---
# Run the interactive world at quarter-native simulation resolution

Replace the half-native interactive target with a 480x270 simulated view upscaled to 1920x1080, preserving world scale and interactivity. Retain half-native as benchmark/reference.

## Acceptance Criteria

Quarter-native config, build and smoke tests pass; 60-frame representative benchmark records frame, fluid and granular GPU times; interactive demo opens at 1920x1080 and painting/camera input work; full make test passes.

