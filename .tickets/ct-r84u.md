---
id: ct-r84u
status: closed
deps: []
links: []
created: 2026-11-26T10:17:50Z
type: bug
priority: 1
assignee: kmd
parent: ct-xsll
tags: [player, beaver]
---
# Keep the beaver walk silhouette connected and articulate the paws

The Vulkan sandbox walk capture shows a transparent horizontal seam across the robe. The generated poses translate the upper rows vertically at a hard boundary, and the paw-stride threshold is below the last visible paw row in the source idle frame.

## Acceptance Criteria

All six derived walk frames retain 4-connected robe rows and their shared support baseline. The two visible lower paws change their horizontal separation during the walk without changing canonical pixel identity. Verify the live Vulkan framebuffer and retain the supplied chew/cast art exactly.


## Notes

**2026-11-26T10:21:19Z**

RED 71b6af5e compiled and reproduced both the robe gap and the unchanged paw separation (10 pass, 2 fail). GREEN fc0164d1 passes all 12 asset checks. The final live Vulkan framebuffer checks all six sandbox frames and reversal, plus the original idle pose in fire-ray mode (4/4). The exported walks each retain all 422 visible idle source IDs exactly once; exact chew/cast hashes still pass. Cropped actual runtime comparison: build/agents/beaver-player/sustained/runtime-walk-review.png.
