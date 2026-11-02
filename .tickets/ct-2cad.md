---
id: ct-2cad
status: open
deps: []
links: []
created: 2026-11-02T15:35:16Z
type: bug
priority: 2
assignee: kmd
tags: [fire, gas, display]
---
# Fix three combustion_gpu_display_tests failures on this NVIDIA machine

make-tested display variant build/combustion_gpu_display_tests reports 16 passed, 3 failed on NVIDIA 580.178.04 (RTX-class GPU, X11 session). Failures: (1) test_retained_char_can_finish_burning_on_the_gpu at tests/vulkan/combustion_gpu_tests.c:61 asserting (g->gas->cache_valid)==(material!=DC_MATERIAL_WOOD); (2) test_empty_air_does_not_reflect_direct_light at line 606 asserting total>0&&light>0 (zero propagated light); (3) stone_and_clearance containment case at line 39 asserting fuel[0]==39322&&fuel[1]==39322&&fuel[2]==39322&&bridge_temperature>320.f (bridge 1400K ignited wood consumed fuel, expected unburnt). Diagnose whether each is display-mode-only (presentation build compiles/uses extra pipelines) or a real physics regression, then fix. No existing ticket owns these assertions.

## Acceptance Criteria

All three assertions pass in build/combustion_body_feedback_display_tests sibling build on the same GPU; combustion_gpu_display_tests reports 19 passed, 0 failed; no regression in make test.

