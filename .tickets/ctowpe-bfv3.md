---
id: ctowpe-bfv3
status: in_progress
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
