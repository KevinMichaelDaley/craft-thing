---
id: ctlo-h9of
status: in_progress
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

**2026-11-01T17:20:03Z**

Reopened on user clarification: fire must visibly illuminate scene geometry, including ordinary and Greek flames. New rendered-pixel RED test first found stale cached dark field after ignition and only +1 red display level on nearby stone. GREEN invalidates scene and seed revisions on combustion enable, accepted physical steps, standalone combustion and emission chunk clearing; no per-flame CPU list. Display exposure calibrated from 0.25 to 4.0 in the visual path only. Focused ordinary test now brightens adjacent stone by >=8 RGB levels; mixed Greek test brightens nearby stone by >=8 green levels with green dominant, with saved diagnostic BMP captures build/ordinary_fire_emissive.bmp and build/greek_fire_emissive.bmp. Existing lighting regressions 19/19 pass. Final full suite and native frame benchmark pending.

**2026-11-01T18:36:06Z**

Replaced the entire directional scan lighting path with finite-range radial point/fire transport and local sky AO. Fire energy aggregates on the GPU in 8x8 tiles; no CPU flame list. Weighted area-emitter rays give graded local penumbra while opaque/unknown walls fully block. Empty air keeps a fixed dark background; water retains its light response. Focused renderer 19/19, rendered clear-air and Greek-fire radial cases 1/1 each. Native X11 quarter-native 1000-frame lit bonfire mean/worst 62.229/68.650ms, paused pixels identical, capture build/bonfire_h9of_radial.bmp inspected: no straight beams or air seam; user approved bonfire look. Catapult Xephyr display 2/2 passes with burning battlement and visible braced frame. Multiframe frame-budget work tracked in ct-ouu4; existing directional performance ticket ctr-ebl5 linked. Full make test pending before close.
