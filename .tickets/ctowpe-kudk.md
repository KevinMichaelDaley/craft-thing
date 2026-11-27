---
id: ctowpe-kudk
status: in_progress
deps: []
links: []
created: 2026-11-27T15:57:04Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Keep native hydraulic aperture scratch in device-local memory

The half-native downward setup took 15.6–15.8 ms on shared host staging and 0.128 ms when both scratch buffers landed in local memory. Allocate dedicated native transport storage sized to the grid and both aperture tree orientations while preserving host-readable component labels, seam owner layout, and exact transport.

## Acceptance Criteria

Native transport does not modify terrain component labels. GPU transport/reference and coupled water tests pass. Repeat the same saturated camera diagnostic and compare downward setup under the same workload.

## Notes

**2026-11-27T16:07:10Z**

Verified RED on the real native water recorder: terrain component labels changed. GREEN has dedicated device-local prefix and tree buffers, retaining mapped terrain labels and the seam descriptor ABI. After repairing the missing incremental build prerequisites for projection and transport, synchronization validation reported zero errors on the native label isolation and both native/seam equivalence cases (3/3). All four direct aperture and ten transport reference tests passed. On the same half-native capture, downward setup was 0.1258/0.1255 ms, versus 15.7854/15.6426 ms in the matched staging baseline. The 16-owner near diagnostic averaged 95.012 ms, p95 121.770 ms. Additional saturated camera bands, final regression runs and full-suite verification are underway.
