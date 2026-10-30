---
id: ct-xomu
status: closed
deps: []
links: []
created: 2026-10-30T19:20:57Z
type: task
priority: 1
assignee: kmd
parent: ctfs-5gil
tags: [performance, save, vulkan]
---
# Batch bonfire shutdown GPU readbacks

After removing the duplicate body checkpoint, shutdown still spends ~8.3s scheduling 187 dirty chunks. Stage trace: chunk download1.7s, color readback2.5s, reactive export2.3s, water0.4s, gas1.2s, queue0.2s. Repeated per-slot GPU readback and queue boundaries dominate. Batch stable state into host staging once where safe, preserve exact snapshots and late-load semantics.

## Acceptance Criteria

One-frame half-native bonfire save flush materially below13s with identical restored chunks, color, reactive, water, and gas; full regression and validation pass; no runtime frame regression.


## Notes

**2026-10-30T19:41:44Z**

GREEN: bulk terrain color snapshot uses host staging once at shutdown; per-chunk water colors remain coherent CPU reads. Late loads invalidate only their slot snapshot. Focused body color test compares both chunks against direct readback (2/2); displayed half-native fast-color save test passes. Eight-frame trace: color stage4ms vs prior2499ms, chunk scheduling5.833s vs8.278s, save flush11.279s vs12.890s, wall16.63s vs17.77s. Full regression pending.

**2026-10-30T19:56:50Z**

Full tools/with-vulkan make -j8 test PASS after staged color snapshot; focused tools/with-vulkan ./build/body_color_tests 2/2 and DC_ASSERT_FAST_COLOR_SAVE=1 displayed test pass. Actual eight-frame half-native run 960x540: color stage4ms, save flush11.279s, wall16.63s. Frame mean29.130ms worst31.562ms over eight frames; this does not meet 32-frame mean+worst<30 and ctfs-5gil remains in progress.
