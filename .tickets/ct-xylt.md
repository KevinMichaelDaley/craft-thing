---
id: ct-xylt
status: closed
deps: []
links: []
created: 2026-10-30T09:35:27Z
type: task
priority: 1
assignee: kmd
---
# Reduce cold and fixture bonfire initialization latency

On RTX A2000 X11, persistent Vulkan pipeline cache reduces repeat GPU startup from about 74s to about 2s, but a cold cache still spends tens of seconds compiling physics and thermal pipelines. The half-native bonfire fixture then takes about 7.2s to upload 187 chunks, including 5.0s for reactive imports. Optimize first launch and fixture admission without altering scene state or frame physics.

## Acceptance Criteria

Measure cold and warm startup separately; reduce cold shader compilation and fixture admission with correctness tests for reactive state and restored chunks; retain device-safe cache behavior; run displayed X11 bonfire and record wall times.


## Notes

**2026-10-30T10:10:02Z**

Half-native RTX A2000 warm-cache startup after monotonic-ID fast path and one arena reservation: GPU window 0.893s, 187-chunk fixture terrain 1.519s, colors 0.051s, reserve 0.187s, reactive imports 2.898s, gas enabled at 5.810s. Previous reactive import was 5.039s and fixture upload 7.177s. Full cold shader compilation remains unresolved; frame still 1097ms and tracked in ctfs-5gil. Reactive chunk GPU tests 5/5 and bonfire fixture CPU tests 3/3 pass.

**2026-10-30T10:31:59Z**

Final wrapped make test after reservation changes passed all preceding tests, including bonfire fixture and reactive chunk cases, then stopped at NVIDIA contact_quad_tests bitwise geometry assertion (ct-4gtu). Targeted bonfire_display_tests on X11 fails frame dt<=0.25f guard, tracked ctfs-5gil. Startup measurements are from the actual 960x540 displayed executable.

**2026-10-30T21:09:24Z**

RTX A2000 generated pipeline cache grew to 70,030,174 bytes, above old 64 MiB save cap; every shutdown silently skipped persistence. Increased device-bound cap to 128 MiB. Opt-in persistence regression RED before fix and GREEN after. Warm displayed 960x540 bonfire now reaches gas enable at 3.074s (GPU window 1.041s; fixture uploaded 2.975s), with complete cache saved. Cold compile remains open.

**2026-10-30T21:30:09Z**

After cap fix and query refactor, full tools/with-vulkan make -j8 test PASS. Subsequent actual displayed 960x540 bonfire loaded 70,334,447-byte cache (loaded1), GPU window1.054s, gas enabled2.972s. Cache save query 70,334,447 bytes within128MiB. Cold first launch still requires pipeline compilation and remains open.

**2026-10-30T21:55:29Z**

2026-10-30 cache survives make clean: default device/driver/UUID file moved to XDG_CACHE_HOME/dungeoncraft (or HOME/.cache/dungeoncraft), with one-time legacy build/ file load and bounded128MiB save. Explicit DC_PIPELINE_CACHE_DIR supports isolated test path. RED opt-in outside-build test failed old code; GREEN test passed. A second actual 960x540 displayed launch loaded 70,334,447-byte user cache, GPU window1.046s and gas enabled3.006s. Full tools/with-vulkan make -j8 test PASS after refactor; focused outside-build and persistence1/1 each, displayed catapult2/2. Cold first-ever compile remains unresolved.

**2026-11-26T15:25:12Z**

The shared RTX cache was observed at 134108241 bytes near the 128 MiB cap. During the timed adhesion tests, the repeated launcher command spent substantial CPU time between scenes using that shared cache. The actual locked siege sweep uses a worktree-local DC_PIPELINE_CACHE_DIR. Its first context loaded zero bytes, saved 22922613 bytes, and subsequent fireproof and delayed contexts loaded and reused that cache. The prompt and stone-carrier controls passed. This is execution evidence for cache isolation across concurrent worktrees.

**2026-11-28T04:57:21Z**

Cold-start follow-up: thermal enable creates the 247,884-byte generic thermal shader pipeline and 24 runtime specializations. Phases 24 and 25 still bind the generic pipeline. In an isolated build, freezing the phase at GLSL compilation and eliminating unreachable functions produced valid SPIR-V for phases 0/1/4/11/24/25 at 98,140 / 105,444 / 102,652 / 25,372 / 55,972 / 55,972 bytes. A direct inlining experiment enlarged some modules to 370,164 bytes. These are module-size checks only; cold and warm startup, emitted heat and conservation still need controlled measurement before selecting this approach.

**2026-11-28T05:13:00Z**

Displayed 960x540 RTX A2000 bonfire on persistent Xephyr, empty application cache and driver disk caching disabled for cold runs. Before/after base-ready plus scene gas-enable intervals totaled 173.993 / 139.671 s cold and 19.583 / 8.271 s warm. The cold base interval itself rose from 87.457 to 95.684 s, while the scene interval fell from 86.536 to 43.987 s. These single runs include concurrent machine load. Candidate uploaded all 187 chunks, passed all four displayed checkpoint cases and both specialized/universal shader-load checks. The separate displayed frame budget remains open.

**2026-11-28T05:14:58Z**

For cold fixture admission, upload-complete to rigid-admitted dropped from 52.693 s to 15.189 s. The upload stage itself took 5.122 / 6.503 s before/after in this pair. The preceding arena reservation separately reduced reactive import on the original comparison from 5.039 to 2.898 s. Restored-parcel/liquid/heat and failed-checkpoint preservation tests passed. Cache reuse loaded all 25,128,675 bytes on the candidate warm run. The displayed executable returned 1 for the independent frame-time gate on both candidate runs; this closes startup only.
