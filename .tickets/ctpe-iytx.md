---
id: ctpe-iytx
status: in_progress
deps: []
links: []
created: 2026-10-14T03:22:18Z
type: task
priority: 1
parent: ctpe-6m89
tags: [libresprite, assets, animation, props]
---
# Verify and pin actual LibreSprite scripting compatibility

Acquire official LibreSprite locally without overwriting system tools, pin version/source and probe actual JavaScript API/CLI with files. Verify layer/cel/image access, offsets, linked frames, palette, durations/tags, filesystem output and nondestructive snapshot support.

## Acceptance Criteria

Actual LibreSprite binary executes probe and preserves source; script/CLI support is measured not assumed from Aseprite Lua. Real-host tests and reproducible install/build/version instructions exist; no mock-only support claims.

