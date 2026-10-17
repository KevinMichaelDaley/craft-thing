---
id: ct-yd8t
status: in_progress
deps: []
links: []
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
