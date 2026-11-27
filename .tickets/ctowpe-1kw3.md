---
id: ctowpe-1kw3
status: closed
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

**2026-11-27T18:39:30Z**

Completed rotating-owner Vulkan timestamp measurements on d50c4f60, sampling one real owner per replay round across 60 measured frames per band. The six water phases accounted for 79.41%, 78.03%, and 83.33% of sampled stage time near/middle/far. MPM accounted for 20.26%, 21.59% and 16.32%, even though each sealed offscreen room in this fixture had water and no grains. Rigid, refresh and bulk together were below 0.4% in every band. Water phase 3 (pressure and the start of transport) was the slowest water phase in all bands at 9.124/8.275/8.460 ms total sampled. Next remove provably idle MPM work for independently batched rooms, then profile the active water projection/transport kernels if p95 remains over budget. The trace preserves 16 owners, 2.000000104 s per band and the 1920x1080 swapchain.
