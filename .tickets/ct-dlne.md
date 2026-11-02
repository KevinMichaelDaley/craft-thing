---
id: ct-dlne
status: in_progress
deps: []
links: []
created: 2026-11-02T00:53:24Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
---
# Crop derived beaver poses and ground the authored feet consistently

Preserve original ASE. Crop transparent runtime bounds, preserve all visible 80-wide cast artwork, use explicit fur-foot baseline independently of tail/effects, common support anchor across phases/facing with canonical owners retained. Verify physical feet on actual ground in live app.


## Notes

**2026-11-02T01:10:35Z**

Derived crop metadata passes all 10 asset cases: chomp/idle foot edge32, cast edges32/33 by reviewed paw rows, excluding tail/staff. Runtime atlas now packs exact per-frame bounds and retains source IDs; mirror offsets preserve original canvas anchors, including all 80-wide cast artwork. The actual Vulkan regression passes with grounded idle/wide cast and no diagnostic ghost. One brown fringe pixel was investigated: actual displaced DIRT from walking, not stale beaver rendering; visual checker now verifies authored pixels and empty-air footprints while allowing real displaced terrain. Original ASE hashes unchanged. Collision/exposure still use canonical rest geometry, so full animated thermal deformation remains pending on ct-xsll.
