---
id: ctr-r0zy
status: closed
deps: []
links: [ctr-pavb]
created: 2026-10-18T00:17:52Z
type: bug
priority: 1
---
# Preserve attached leaves when the airbrush cuts only wood

## Acceptance Criteria

Trunk-only airbrush edits retain attached leaf pixels and colors with no leaf debris; direct foliage damage still produces leaf fragments; failed carve reservations remain atomic; Vulkan regression.


## Notes

**2026-10-18T00:25:51Z**

RED reproduces trunk-only brush removing all eight attached leaves. Track leaf-material hits separately from general body hits; only foliage hits reserve/publish leaf debris. Wood-only cuts retain leaf pixels and custom colors. Seven carving regressions pass under core/sync validation; updated positive crumble test explicitly hits foliage (three erased leaf pixels, five emitted flakes), retains wood and validates reservation rollback/color save-load. Actual level03 follow-up trunk cut removes wood while attached leaves remain3526 ->3526; 450-tick scene core/sync validation passes with zero upward wood COM and zero sampled stone overlap. Final full suite in progress.

**2026-10-18T00:29:37Z**

Final full make test completed: seven carving cases pass including attached leaf RGB preservation; only the two pre-existing authored_stage_tests failures at67/135 remain (ctr-ojvy). Graphical scene validates3526 ->3526 attached leaves after a verified wood-removing stroke. Existing saved world retained; SIGTERM shutdown can lose unsaved active edits, separately tracked ctr-pavb. Fresh review scene uses a separate world directory.
