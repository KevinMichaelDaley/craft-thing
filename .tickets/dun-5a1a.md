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
