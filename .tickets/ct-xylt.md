---
id: ct-xylt
status: in_progress
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
