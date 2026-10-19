---
id: ctr-dtib
status: closed
deps: []
links: []
created: 2026-10-17T21:25:19Z
type: task
priority: 2
---
# Import paired PNG color and material maps as terrain scenes

## Acceptance Criteria

Preserve exact color pixels and alignment; explicit palette mapping; reject mismatched sizes and unknown labels; validate package and run placement demo.


## Notes

**2026-10-17T21:36:21Z**

Source PNGs found in prop-export worktree; copied unchanged. Explicit user palette recorded in level_piece03 profile; green is new leaves material (ctr-q1ui). Common lossless occupied crop 439x328 fits resident placement; exact RGB/material alignment tests pass, whole prop asset suite passes. Export and reusable demo launcher checked into repository pending final Vulkan scene run.

**2026-10-17T21:46:49Z**

Complete: imported unchanged paired PNG sources with explicit user palette and lossless common crop; package validated and source RGB alignment tested. Full make -j4 -i test completed with only the two pre-existing ctr-ojvy authored-stage REDs. Wayland Vulkan demo is running level_piece03 (439x328), centered chunk(0,0), separate world_chunks_level_piece03, automatic physics. Reusable tools/demo_level_piece03 launcher and importer are in repository.
