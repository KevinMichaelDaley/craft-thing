---
id: ct-9ieb
status: in_progress
deps: []
links: []
created: 2026-11-02T01:02:23Z
type: feature
priority: 1
assignee: kmd
parent: ct-exwr
---
# Playable beaver fire-ray demo with massive unlit right-edge wood bonfire

Distinct build/beaver_fire_demo executable and Make target. A/D movement, Space jump, Q facing fire ray committed once at cast frame six, no auto casting or generated walk art. Real ray traverses ignitable materials/air and stops at fire-blocking matter, changes actual thermal/reactive state, ignites massive initially unlit rightmost wood bonfire with real flames/smoke. Preserve ground alignment, clean rendering, source ASE and pixel ownership. Document reproducible run and verify actual window plus blocker/material and intent regressions.


## Notes

**2026-11-02T02:00:51Z**

Named executable build/beaver_fire_demo and beaver-fire-demo target now integrated. Ten rightmost ambient293.15K logs (8400 occupied wood cells) are real independent rigid objects with imported reactive parcels; actual gas/combustion supplies heat/flames/smoke. Q uses facing ray, generation-bearing emitters, profile-based fuel traversal, nonfuel/water/missing-page blockers. Real Vulkan direction/blocker and retry tests pass3/3 (including idle no-work). Actual window integration passed: pile fuel330304800 ->330293948, char2140,79 flaming parcels, real gas exhaust positive. A single covered/random-threshold parcel remained hot but unignited, so verification correctly aggregates the whole pile and checks actual combustion products. Fresh user demo PID41100 is open on Wayland; real user Q press logged frame6 event1. Latest timing explicitly80ms/frame, emit400/total640; 16 state,10 asset,7 input,4 SDL,3 clock and3 profile CPU checks pass, actual runtime irregular presentation timing test passes. Full GPU rerun deferred while user actively tests to avoid competing renderer workloads. Remaining GPU-heavy demo frame cost is tracked ct-b42x, not hidden by reducing scene/material quality.
