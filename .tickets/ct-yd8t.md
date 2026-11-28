---
id: ct-yd8t
status: closed
deps: []
links: [ct-hhmy, ct-wuci]
created: 2026-10-17T07:42:34Z
type: bug
priority: 1
parent: ct-wuci
tags: [vulkan, extraction, buoyancy, interactive]
---
# Detached painted stone stays terrain over water in the interactive sandbox

User reports a large stone piece cut from connected mainland remains gray and appears to float in water in the live quarter-native material-painting sandbox. Gray is static terrain, so prove the exact extraction gate rather than assuming a buoyancy force error. Preserve user world; no GPU tests while its sandbox is open.

## Acceptance Criteria

Use actual dc_level_view paint and eraser with large four-connected known island spanning chunks over water, sourced from connected mainland. Assert source cleared/body birth, exact occupied count/materials, stone mass2.7*N and dynamic inverse flags, then downward COM motion without hidden support/grain contacts; matched wood floats. Inspect actual residency, dirty-after-cut, UNKNOWN, candidate/body/arena saturation and expose understandable extraction reasons. Keep conservative cold handling, exact geometry and atomic reservations. Follow RED/GREEN/refactor and full appropriate validation, commit/push.


## Notes

**2026-10-17T07:43:14Z**

User explicitly rejects full visibility as an extraction or gravity requirement. Camera/view culling must not determine physics eligibility: isolated components in known loaded/cached world data must extract and move when partially or wholly offscreen. Add camera-invariant birth/dynamics regressions, not only cached birth. Truly unloaded connectivity remains UNKNOWN; stream/request needed world data or defer with a reason, never treat viewport edges as physical roots. Prior fully-visible troubleshooting wording was not the intended requirement.

**2026-10-17T07:43:20Z**

User additionally rejects viewport/full-visibility eligibility. Fully known loaded/cached world components must extract and simulate whether visible, partly visible or wholly offscreen. Rendering culling must be separate. Conservative UNKNOWN remains only for genuinely missing world connectivity. Source audit confirms birth scans global resident registry atlas, but rigid_solver.comp awake currently rejects body.visible==0, rejects reference outside simulation grid, then requires mapped page; rigid.comp locate_body uses grid intersection. This is an actual cached-dynamics gap, distinct from unproven live gray-stone rejection. Add camera-independent birth/mass/dynamics RED cases across cached seams and true missing-neighbor diagnostics. No GPU work while live sandbox open.

**2026-10-17T07:47:12Z**

Behavioral RED executed after sandbox closed: onscreen known stone128pixels mass345.600006 vy65536 passes; partly offscreen cached stone x60 incorrectly vy-11439; wholly offscreen known cached seam x124 vy0.1PASS2FAIL, zero Vulkan/sync errors. Actual streamed dc_level_view paint+eraser3200pixel cross-seam mainland pond fixture passes stone+wood2/2: stone mass8640 density2.7 COMy-47.842529→-13.760376, wood mass1920 density.6 COM-48.166748→-70.105347; source cleared,dynamic flags,nooverflow/no solvedcontacts. Therefore generic large-stone hydro is not reproduced; user gray terrain still requires exact saved-world eligibility/reservation evidence. Saved user world preserved untouched. Test scheduler remains paused while prioritizing this bug.

**2026-10-17T07:48:17Z**

Offline read of the exact user-saved world proves all64/64 body records active and owned (IDs0x80000001..0x80000040): slots0..62 bounds7x7, slot63 bounds28x11. Arena uses only2722words, so body-slot exhaustion—not geometry arena or stone density—is the immediate birth blocker in saved state. Do not delete/merge bodies or increase constant as a substitute for real resource scaling. Held paint invokes material brush each rendered frame (main.c1280), and brush writes terrain under existing rigid occupancy; need deterministic repeated-stamp regression and explicit saturation diagnostics. Cached visibility RED remains a separate real dynamics defect.

**2026-11-28T07:26:09Z**

Current native cached-motion recheck under Vulkan synchronization validation: all three fixtures created an exact 128-pixel stone body of mass 345.600006. After four rigid steps, vertical velocity at x20 was 65536, at x60 it was 10794, and at x124 it remained zero. The wholly cached body still freezes. rigid.comp locate_body replaces off-view positions with a sentinel and rigid_solver.comp awake rejects bodies using viewport visibility and its page table. A camera-independent update must also retain real world-space neighbors, contacts and conservative UNKNOWN handling.

**2026-11-28T07:40:36Z**

GPU seam regression: a 128-pixel stone body (mass 345.600006) now reaches vertical velocity 65536 after four steps both on screen and straddling a known cached chunk. Cached floor candidates and exact contact rows are checked, and recycling the chunk slot restores boundary contacts. Wholly cached stone still has zero vertical velocity.

**2026-11-28T07:46:20Z**

Native Vulkan regression with the camera moved one chunk right exposed a second seam failure: the same partly visible 128-pixel stone had zero vertical velocity when its first occupied pixel was left of the viewport. The solver now resolves that signed cached reference. Both camera positions give 65536 after four steps; evicting the reference chunk preserves the last world position and velocity. Representable wholly offscreen bodies now retain correct solver coordinates; x124 velocity remains zero. Residency results 3 passed, 1 failed. World body tests 3/3, including far-camera eviction and signed world carry.

**2026-11-28T07:58:21Z**

Broadphase and exact contacts now cover a fully cached 128-pixel stone across chunks 1 and 2. Native GPU verification resolves the cached floor at its actual cell row, switches to boundary on eviction, and finds the restored floor in a different slot. These checks also pass after rebasing the camera to (+2^40,-2^40) chunks with no displayed page. A box spanning the seam produces one body pair and exact contacts; a resident decoy offset by 2^32 chunks produces none. Offscreen motion remains open: after four steps x124 still has zero velocity.

**2026-11-28T08:17:35Z**

Native validated solver now advances the 128-pixel stone at x124 to vy=65536 after four rigid steps, matching both viewport edges and the on-screen case. Submerged wood in a 0.5-cell/tick current reaches vx=1640, vy=-10920 in each camera position, including a camera rebase of (+2^40,-2^40) chunks. Its canonical position and velocity agree. A distant cached floor arrests a falling body at y=27.999481 for a floor at 36 and an 8-cell hull. The application paint-and-erase fixture clears the entire 3200-pixel source and checks stone/wood masses; stone sinks and wood rises, but wood reports 16 contacts during the final tick. An isolated checkout at 09f3ac53 reproduces the same trajectory and count. The zero-contact acceptance gate remains open.

**2026-11-28T08:22:07Z**

The native paint-and-erase fixture now passes all four matched trials. The original one-cell cut left a stone tip at world (78,-48) which wood grazed sideways while rising. With a wider cut both 3200-pixel pieces clear the bank, stone sinks and wood rises with zero solved contacts over all 20 ticks. All eight camera/cached residency tests also pass, including distant world floor and water flow.

**2026-11-28T08:28:59Z**

Final quarter-native validation: the actual eraser marks extraction pending, all 70 visible/halo chunks are available, and every one of the 3200 unique body-local cells has the correct rigid material in all four trials. The original 64-owned-slot save diagnosis is recorded above. The F3 sandbox report exposes residency, dirty/pending state, unknown connectivity, batch backlog, body-slot and geometry admission reasons. A 71-island native case reports 64 selected and seven deferred. Five diagnostic and four native extraction cases pass, including atomic retry and warm/cold seams. All eight camera-residency cases pass. Native world-body, solver, exact contact, broadphase, revolute and grain-bed suites passed. Both extraction shader variants passed SPIR-V validation. The ongoing repository-wide run and separate original level03 replay are tracked independently.
