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

