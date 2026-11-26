---
id: ct-d7q8
status: in_progress
deps: []
links: []
created: 2026-11-26T10:22:07Z
type: bug
priority: 2
assignee: kmd
parent: ct-exwr
tags: [player, beaver, tests]
---
# Read split-screen reticle pixels from the test window

test_player_split samples absolute desktop coordinates using SDL_GetKeyboardFocus and ImageMagick import -window root. The reticle assertion failed at tests/app/player_split_tests.c:284 while the live animation window ran concurrently. On the same implementation an isolated Vulkan rerun passed 8/8. Global focus, compositing and occlusion can change the sampled pixels.

## Acceptance Criteria

Verify the actual rendered reticle in its owning viewport without using global keyboard focus or root-window pixel sampling for the assertion. Verify the second viewport and disconnected player remain clear, and exercise the test while another window is present.


## Notes

**2026-11-26T10:23:28Z**

The Vulkan presenter adds the reticle to the swapchain image after the viewport blits. dc_level_view_pixels reads the earlier scene image, so a window-independent pixel check needs a sample after reticle composition.

**2026-11-26T11:38:07Z**

The strict run pinned to 2d63f902 stopped with 7/8 split tests. The root-window PNGs before and after the walk were byte-identical while a separate runtime Vulkan test used the same X display. The framebuffer reticle case itself passed in that run. Further pose tests use the nested :92 display to avoid desktop overlap.
