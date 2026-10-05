---
id: ct-rp2f
status: closed
deps: []
links: []
created: 2026-10-05T01:03:17Z
type: task
priority: 0
parent: dun-vd9a
---
# Add quarter-native Vulkan view and benchmark

## Acceptance Criteria

480x270 physics cells present at 1920x1080 using 4x scaling; 640x448 simulation including halo fits exactly 70 resident chunks; world proportions, brush and pointer mapping match display scale; native Vulkan smoke and water/grain benchmark run on local GPU.


## Notes

**2026-10-05T01:22:32Z**

Implemented 480x270 view at 4x display scale, 640x448 simulation grid and exactly 70 resident chunks, matching brush/world proportions and separate world storage. Quarter-native window smoke passed on Intel Iris Xe through Wayland, including corner paint, GPU screenshots, full residency and chunk panning. Build targets and documentation added. All 90 headless tests plus quarter-native fluid/window/long spray validation pass; no Vulkan validation errors or warnings.
