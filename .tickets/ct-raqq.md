---
id: ct-raqq
status: closed
deps: []
links: []
created: 2026-10-30T19:12:03Z
type: task
priority: 1
assignee: kmd
parent: ctfs-5gil
tags: [performance, save]
---
# Reduce bonfire shutdown save latency

Half-native one-frame bonfire reaches gas enabled at 3.074s but process wall time is 28.17s. Find dominant phase in final structure/chunk/gas checkpoint save and reduce the 25s exit cost while preserving persisted state and no dirty chunks.

## Acceptance Criteria

Profile stage timings; reduce one-frame bonfire exit wall time materially with identical persisted state; shutdown and full regression tests pass; changes pushed.


## Notes

**2026-10-30T19:14:46Z**

Startup trace one-frame half-native: gas enabled3.015s, wall28.97s. DC_SAVE_TRACE: final body structure5.828s, schedule_saves repeats body structure9.247s, chunk export/queue8.857s, total flush23.947s. RED opt-in test catches repeated structure checkpoint. The same structure is saved twice during destroy when dirty chunks are queued.

**2026-10-30T19:19:02Z**

GREEN: shutdown reuses the first structure checkpoint, invalidating only when a late load is published. Direct eight-frame half-native run had final structure4.447s, repeat schedule0ms, chunk saves9.057s, total save flush13.522s and process wall18.74s versus baseline28.97s. Opt-in displayed shutdown test now counts late loads and passes. Full regression and stage cleanup pending.

**2026-10-30T19:35:48Z**

Full tools/with-vulkan make -j8 test PASS after GREEN and save-stage trace. Final one-frame trace: body structure4.599s, repeated structure0ms, chunk scheduling8.278s (download1.723s colors2.499s reactive2.254s water0.398s gas1.174s queue0.225s), save flush12.890s, process wall17.77s. Late-load invalidation remains covered by displayed test. Remaining batch readback work filed ct-xomu.
