---
id: ctowpe-ysgp
status: in_progress
deps: []
links: []
created: 2026-11-27T17:40:40Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-o0i2
---
# Use a presentation-only Vulkan device for local X11 software presentation

On hidden Xephyr the RTX client sends full frames through the X socket. An isolated second Vulkan device can present the RTX-composed image through the same SDL window. Integrate a per-window path with conservative capability checks and retain native presentation when the alternate path is unavailable.

## Acceptance Criteria

Keep RTX physics and composition, the real full-size swapchain, presentation cadence and capture semantics. Verify byte-exact swapchain readback, visible pixels, multiple windows, partial setup cleanup, resize or recreation as supported by the existing window path, and fallback. Re-run the production dense and exact concave 720 tests and both saturated camera modes under hidden Xephyr. Keep the overall frame budget open for any failing band.


## Notes

**2026-11-27T17:40:56Z**

The corrected diagnostic at 1abdfaa8 used the RTX A2000 for the complete 1280x720 exact concave simulation and composition, with llvmpipe only for the FIFO swapchain. All 3686400 bytes read back from that swapchain matched the RTX-composed image. With synchronization validation it reported no synchronization hazards or VUIDs (29.331 ms mean, 34.280 ms p95). The normal performance run passed at 19.068/23.333 ms. No runtime backend has shipped. The original benchmark deliberately hides its SDL window, so its pixel check reads the actual Vulkan swapchain. The mapped camera fixture also checks pixels visible inside Xephyr.
