---
id: ctpe-iytx
status: closed
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


## Notes

**2026-10-14T03:54:31Z**

Complete: official LibreSprite1.3 Linux AppImage SHA256 verified, source tag pin documented, shipped local installer executed. Actual host probe verifies hidden locked metadata, offsets, linked cels, raw pixels, copy snapshot and original preservation. Actual scripted RGBA/indexed examples preserve exact channels/durations/tags. Pinned API lacks current-upstream fs and direct timing/tag access; verified snapshot adapter used. Host drops fractional indexed palette alpha: diagnosed/rejected with RGBA conversion requirement.
