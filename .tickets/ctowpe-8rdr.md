---
id: ctowpe-8rdr
status: in_progress
deps: []
links: []
created: 2026-11-27T18:04:56Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Submit due sealed camera workspaces together on their shared Vulkan queue

In the 47e99db4 half-native near-band profile, every slow frame advanced all sixteen owners serially. Offscreen work averaged 35.33 ms on those frames. Restore the shared submission for independent due rooms from archive 73475311 against the current Vulkan and confirmed-clock APIs.

## Acceptance Criteria

Exercise the real camera path with multiple independently due owners. Record each eligible owner's unchanged step and submit their command buffers together. Retain exact individual timestep partitions and stocks, confirm clocks only after GPU completion, and preserve error handling, gas, coupled seams, returning rooms and ordinary one-owner advancement. Use validation and measure the saturated camera on hidden Xephyr.

