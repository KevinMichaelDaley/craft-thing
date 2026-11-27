---
id: ctowpe-bfv3
status: closed
deps: []
links: []
created: 2026-11-27T15:06:24Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Batch resident presentation steps while sealed disk owners replay independently

The measured saturated path sends two separate resident tick submissions and then presents a zero-step frame. Use the regular equal-step presentation batch for independently owned sealed disk rooms. Preserve per-step coordination for liquid seams, rigid owners, pending continuations, mixed elapsed times and player updates.


## Notes

**2026-11-27T15:08:55Z**

The regression reproduced two resident steps outside presentation while an independent disk owner was present. All 29 host timing tests now pass, including a non-disk handoff that still checks the seam per step. Waiting on synchronization-validated camera cases and exclusive half/native frame comparisons.

**2026-11-27T15:25:15Z**

Post-change GPU synchronization validation passed all 5 camera checks, including coupled heat, saturation and reversal. Half-native near/middle/far mean and p95 (ms): 143.243/195.334, 135.919/191.281, 133.318/195.229. Native: 390.953/964.926, 417.056/1103.494, 420.001/1014.992. Two unrelated graphics applications started after the original baseline and remained active during this run. Cross-run timings alone cannot isolate the batching effect. A near-native within-process comparison alternates both paths with the same warm scene.

**2026-11-27T15:29:23Z**

Same-process native near comparison, 16 owners, 96 frames total, 48 per path with assignment reversed across successive 24-frame blocks. Separate submissions mean/median/p95 413.829/201.268/1065.313 ms; combined presentation 419.207/196.803/1014.216 ms. The host submission count drops as intended. Results do not establish a throughput gain. The 33.333 ms gate remains open under ctowpe-vtlh.
