---
id: dun-5a1a
status: in_progress
deps: []
links: []
created: 2026-09-28T03:12:04Z
type: task
priority: 1
assignee: kmd
parent: dun-0t9m
---
# Prefetch and budget dynamic chunks returning from deep sleep

Track saved dynamic chunks outside the four-screen band. When camera approaches again, stream them into a bounded GPU workspace before they become visible, and recycle workspace capacity without freezing nearby activity.

## Acceptance Criteria

A chunk saved beyond four screens resumes GPU physics upon re-entry to the two-screen band and falls before visible promotion. Saturated multi-directional cache keeps a bounded GPU memory footprint and advances all near chunks on the world clock; half/native benchmark records frame and GPU times.


## Notes

**2026-09-28T17:10:32Z**

Deep-sleep dynamic chunks prefetch into GPU cache before visible return; returning water pan test passes. Offscreen context now 80 slots in a 10x8 chunk workspace: 192.8 MiB at half/native, with 0.2 seconds of GPU simulation in 20-23 ms. Four-cache memory bound is 1.22 GiB half native / 2.24 GiB native including foreground. Saturated multi-directional fairness remains to verify.

**2026-09-28T17:11:11Z**

Remaining acceptance work: saturated multi-directional cache fairness and live near-band budget under camera motion; conservative GPU seam handoff is now closed in dun-bd93. Shared workspaces use 80 slots and no per-frame readback.

**2026-09-30T03:50:16Z**

Quarter-native profile after resolution switch: foreground 168.7 MiB, one 4x4 offscreen workspace 38.6 MiB, 0.2 s offscreen simulation 18.78 ms wall. Current runtime permits 16 offscreen workspaces (src/app/level.c) with 16 slots each (src/app/offscreen.c); prior ticket note about 80-slot contexts is stale. Saturated multi-directional frame budget and fairness remain open; need a representative quarter-native stress regression before closing.

**2026-09-30T04:22:28Z**

Reduced each background cache grid from 4x4 to 3x3 chunks; quarter-native allocation 38.6 -> 21.7 MiB. Batched equal-timestep GPU substeps into one submission per catch-up period (six submissions -> one for 0.2 s near-band advance); representative 0.2 s wall time 18.78 -> 10.46 ms. Existing 20 UI camera/transfer tests pass. Saturated multi-directional fairness and full-ring frame budget remain open.

**2026-09-30T04:23:42Z**

Single cached water chunk, quarter-native diagnostic tick after catch-up: GPU rigid 0.023 ms, fluid 0.619 ms, granular 0.231 ms; 6 GPU substeps cost ~5.2 ms GPU but ~10-12 ms wall including command recording/sync. A fully saturated 16-grid ring would still exceed 60 Hz; next step should pack active background chunks into shared cadence atlases rather than schedule 16 independent command streams. No per-frame copyback.
