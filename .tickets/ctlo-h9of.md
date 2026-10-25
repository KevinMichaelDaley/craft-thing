---
id: ctlo-h9of
status: open
deps: [ctlo-8b67]
links: [ctr-6fii, ctlo-r2ap, ctfs-dmxv]
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Render colored fire smoke and aggregate emissive transport

Render reactive flame emission and gas extinction/scattering using separate chemical state and visual pigment. Add fire energy to the existing aggregate emissive/directional buffer.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Ordinary/magical flame and smoke colors, alpha/coverage and paused frames are stable while chemistry is unchanged. No loop over individual flames as point lights. Fire and smoke remain visible with propagated lighting disabled; optional lighting changes neither physical heat nor oxygen. Compare captures and actual full-frame/GPU/memory costs on the real Wayland Vulkan display.

