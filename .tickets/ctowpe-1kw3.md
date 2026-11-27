---
id: ctowpe-1kw3
status: in_progress
deps: []
links: []
created: 2026-11-27T18:22:05Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Measure the solver stages in batched replay of sealed camera rooms

At d50c4f60, the batched half-native camera p95 remains 42.568/48.291/50.596 ms near/middle/far. Measure the time spent in batch recording and fence waits and sample actual GPU timestamps for the physical stages of the independent owner replay.

## Acceptance Criteria

On hidden Xephyr :93, measure the existing 16-owner, 60-frame camera in all three bands. Separate host waits from resident presentation and rank GPU stage times from actual replay substeps. Preserve both fixed foreground steps, the world clock and the full presentation extent. Identify the next change based on the measured costs.


## Notes

**2026-11-27T18:34:01Z**

The per-frame d50c4f60 wall-clock trace is complete on :93. Each over-budget frame advanced 16 owners in two shared submissions (32 owner commands). Mean over-budget frame/replay/fence/resident+present times (ms): near 39.17/28.12/25.91/10.66, middle 47.31/35.28/32.73/11.81, far 47.61/35.73/33.22/11.48. The batch spent 92–93% of replay time waiting for GPU completion. The separate rotating-owner timestamp probe ranks solver stages without treating host wait as GPU stage time.
