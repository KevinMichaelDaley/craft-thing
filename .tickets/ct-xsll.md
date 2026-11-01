---
id: ct-xsll
status: in_progress
deps: [ct-2p38]
links: []
created: 2026-11-01T19:29:54Z
type: task
priority: 1
parent: ct-exwr
tags: [player, beaver]
---
# Author placeholder beaver animation and persistent material pixel bindings

Create recognizable beaver placeholder art with robe, hat, fur, teeth and tail, plus 6 walk, 8 chewing and 4 casting frames. Supply per-pixel material assignment and stable body/equipment identity across animation.

## Acceptance Criteria

Assets load through supported pipeline with exact frame counts and usable frame timing. Fur, robe and hat have explicit material coverage; transparent pixels are not solid. Reactive state persists by player and material/rest identity through frame changes, flips and movement: no reset by replacing frames. Rendering reflects local singeing, wetness and burning, backed by actual existing reactive simulation. Different player instances never share mutable pixel state. Include asset checks and runtime visual evidence.


## Notes

**2026-11-01T22:33:09Z**

Native asset substage implemented under RED/GREEN: 429e1dbd compiled with 0/7 passing, then 4bc14f32 passes all 7 asset cases plus all 10 player cases. Deterministic 40x40 standard CPRP clips use exact 6 walk, 8 chew, 4 cast frames; all poses distinct. Fur/robe/hat/tail/teeth regions have complete physical coverage, transparent pixels have none. Canonical source maps and BODY/appearance owner contracts retain source identities under frames/flip and distinguish players. All 17 cases also pass ASan/UBSan. Contact sheet generated and visually inspected at build/beaver_assets/contact_sheet.png; this is an asset review, NOT real application acceptance. Provisional physical tuning explicitly aliases wettable combustible wood for fur/robe/hat/tail and stone for teeth; all 15 packed body material IDs are already assigned. Manifest assets/player/beaver/manifest.json records this and source-map lifecycle. Existing source/render/material contracts reused, no 3D workflow or CPU reaction substitute. Remaining before closure: live GPU admission/animation/render adapter, source-to-body origin mapping for trimmed shapes, preserved canonical parcels including occluded texels, material exposure/feedback through existing simulation, and real window evidence across movement/animation. Full suite rerun logging build/agents/beaver-player/asset-full-tests.log.

**2026-11-01T22:39:10Z**

Final native-asset substage cleanup: material aliases centralized, internal helper prefixes follow subsystem conventions, permanent export/contact-sheet diagnostic added, README/manifest/lifecycle documentation complete. Final make test attempt exited 2 at the identical baseline catapult support assertion (tests/app/catapult_scene_tests.c:343; 12 passed/1 failed). No new compiler warnings; inherited recipe override warnings tracked ct-il37. Final targeted make test_player passes 17/17 and exporter builds/runs. This ticket intentionally remains in_progress: no live main/renderer integration was changed. Precise next work is in build/agents/beaver-player/progress.md, especially full-canvas owner helper versus extraction-trimmed body rest origin and retention of occluded canonical parcels. Continue runtime adapter RED tests before ct-ortp.
