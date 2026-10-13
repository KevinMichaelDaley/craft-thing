---
id: ct-alp7
status: in_progress
deps: []
links: [ct-wuci]
created: 2026-10-13T05:17:54Z
type: bug
priority: 1
tags: [gpu, performance, pixels]
---
# Restore real-time performance for complex pixel-body contacts

User stopped ct-wuci implementation agent after the two-concave-pixel-body visual test ran as a slideshow. Manual headless reproduction of the exact scene measures 142-164ms per steady rigid tick, with 130-155ms in contact generation; warm extraction is about 0.15ms. Parallelize exact contact work, retain collision/material correctness, add the exact scene to measured quarter-native performance gates and verify the real display.

## Acceptance Criteria

Exact scene meets existing real-time GPU budget; old serial path fails a compiling RED performance test. Preserve arbitrary pixel geometry, holes, material/contact keys, saturated rollback and body motion. Full correctness/visual regressions and Vulkan validation pass. No approximation, reduced physics resolution, skipped required contacts, CPU physics fallback or unsolicited agent resumption.

