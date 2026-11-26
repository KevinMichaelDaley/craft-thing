---
id: ct-9ieb
status: closed
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

**2026-11-02T02:10:09Z**

CPU-only failure-injection regression exposed consumed cast sequence/duration before successful GPU submission. RED4pass2fail -> GREEN6pass0fail. Host scheduling now staged locally and committed only on successful submission; failed queue boundary/command begin/submit keeps intent retryable, successful retry publishes once, failed active step retains remaining duration. No live executable/resources rebuilt. Full GPU/fullsuite verification remains deferred while user tests PID41100.

**2026-11-26T05:54:33Z**

Verified on the RTX A2000 that the demo retains the authored framing for 330 live ticks, ignites the real pile, and reloads the gas clock and all compared chunk checkpoints exactly. Camera and reload regression tracked and closed in ct-ztym. The remaining frame-budget evaluation stays with ct-b42x.

**2026-11-26T10:21:19Z**

Framebuffer RED c0e3f9cc detects generated walk artwork while the fire-ray mode is active (226 channel differences); GREEN 9d74dc1b restores the supplied idle pose for movement in this mode. The same run checks all six sandbox poses and finishes 4/4. Both scenes use the same canonical player ownership.

**2026-11-26T10:22:51Z**

The isolated live demo regression finished 1/1 after 330 ticks. The real pile consumed 9,938 fuel mass units, produced 1,961 char units and reached 67 flaming parcels. The authored camera, exact restored gas checkpoints and restored pile inventory all passed. The existing ray blocker and intent checks ran in the clean broad Vulkan suite. Frame-time optimization continues under ct-b42x.
