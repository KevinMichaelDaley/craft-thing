---
id: ctpe-b1r2
status: closed
deps: [ctpe-5lmh, ctpe-iytx]
links: []
created: 2026-10-14T03:22:18Z
type: task
priority: 1
parent: ctpe-6m89
tags: [libresprite, assets, animation, props]
---
# Implement nondestructive LibreSprite multilayer export script

Ship menu/script entry point plus verified optional batch adapter. Extract reserved channels across all frames including hidden metadata and offsets, linked/missing cels, timings/tags and palette decoding; stage complete versioned package without flattening original.

## Acceptance Criteria

Actual pinned host runs shipped script on RGB and indexed fixtures; all channels remain synchronized, durations/tags/pivot preserved, unknown labels/duplicates/partial-alpha metadata diagnosed. Saved/unsaved behavior explicit; source unchanged; deterministic and atomic output.


## Notes

**2026-10-14T03:54:31Z**

Complete: real in-app JavaScript export_prop.js captures unsaved active edits nondestructively via fresh unique SaveFileCopyAs snapshot and request; SHA256 per active raw cel + bounds/frame identity checked against saved snapshot before supported offline packer. Real-host tests verify both RGBA/indexed, hidden/locked metadata, linked offsets, original unchanged. Stock host cannot launch Python; documented two-step script+adapter workflow. Atomic package publication validated; runtime game placement remains separate open tickets.
