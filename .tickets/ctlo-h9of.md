---
id: ctlo-h9of
status: closed
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


## Notes

**2026-11-01T16:53:58Z**

Implemented GPU per-cell finite fire emission injection into existing directional aggregate transport (no CPU flame list), preserving sky/point reach with a second fire reach marker. Mixed oil/red-slime film now emits explicit green chromaticity and green-weighted transported smoke pigment without changing reaction energy or oxygen. RED tests cover no-point fire illumination, paused field stability, unchanged gas/wood chemistry, Greek versus oil source flags and pigment; focused fire and existing lighting regressions 19/19 pass. First full make test passed under persistent Xephyr :92. Native X11 Vulkan quarter-native bonfire no-light controls: 1000-frame mean/worst 18.445/21.473ms and 18.734/24.640ms; captures in build/bonfire_measurements_6rthqN. Optional propagated lighting: 63.733/67.250ms mean/worst and 536572KiB peak RSS, fails 30ms budget; follow-up recorded on ctr-ebl5. No Wayland display socket exists in this session. Refactor full suite rerun pending.

**2026-11-01T17:05:03Z**

Final RED/GREEN/REFACTOR full make test passed under persistent Xephyr :92. Clean native X11 Vulkan final-code optional-lighting replay: 1000 frames mean/worst 64.175/68.328ms, paused pixels identical, capture build/bonfire_lit_h9of_final.bmp, fails 30ms budget and is tracked on ctr-ebl5. Clean unlit controls passed at 18.445/21.473ms and 18.734/24.640ms. Detailed timing diagnostics add overhead to unlit frames and were used only for memory: peak RSS 534144KiB unlit versus 536572KiB lit. No Wayland socket was available in this session.
