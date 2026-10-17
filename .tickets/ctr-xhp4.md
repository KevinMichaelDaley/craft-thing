---
id: ctr-xhp4
status: in_progress
deps: []
links: []
created: 2026-10-17T20:31:48Z
type: bug
priority: 1
---
# Resume falling after a second supporting terrain component detaches

## Acceptance Criteria

A detached tree falls and settles, then moves again when terrain supporting it extracts as a second body. Contacts remain bounded without global solver rollback or stale support/sleep states. Add a sequential-detachment regression and verify on Vulkan.


## Notes

**2026-10-17T20:38:00Z**

Saved-world replay reproduced hovering tree body 0x80000001 at rest_ticks=30 with no solver overflow and awake=1. Captured all eight substep contact manifolds: supporting terrain is wood at canonical world (25,-4) and (12,19). Both are one-pixel FOUR-neighbor islands already disconnected in original asset, but touch the tree diagonally. Loaded-terrain hold rule correctly leaves them static. Erasing just those two in-memory restores gravity immediately (vy .25 then .5 then .75). Ordinary two-stage detachment regression passes unchanged. Awaiting user choice between diagonal solid connectivity and edge-connected scene mask correction; do not silently change the loaded-terrain policy.

**2026-10-17T20:41:41Z**

Diagnostic replay passed with Vulkan core and synchronization validation (no VUID/SYNC errors); input snapshot untouched. New staged-detachment test passes. Full make -j4 -i test completed with only the two existing ctr-ojvy authored-stage failures. No engine/asset behavior changed pending connectivity choice; ticket remains in_progress.

**2026-10-17T20:54:26Z**

User correctly clarified that the entire trunk hangs. Expanded diagnostic confirms body 0x80000001 is the full 32x50 trunk/crown, 288 wood pixels, awake=1, flags=0, inverse mass positive; it remains at rest30. Terrain-component inventory and physical overlay distinguish fixed wood from the moving tree. Original single-cell contacts at branch height pin the WHOLE trunk, not merely a separate treetop. support_tree_detail.bmp colors fixed wood magenta. No connectivity/asset change without resolving the existing design question.

**2026-10-17T20:57:51Z**

Expanded full-trunk replay passed core/sync validation without errors. Full make -j4 -i test completed with only the two pre-existing authored-stage failures; no gameplay source changed. Diagnostic now inventories wood in every body and all rigid-capable wood terrain components, plus fixed-wood overlay. Whole 288-pixel trunk/crown body is confirmed suspended by branch-height contacts. Bug remains open; no fix claimed.
