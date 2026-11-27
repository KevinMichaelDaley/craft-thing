---
id: ctowpe-kudk
status: closed
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

**2026-11-27T16:14:48Z**

The half-native production run with dedicated scratch completed all distance bands at 16 owners and the required 2.000000104 s simulated time per 60 measured frames. Near, middle, and far mean/p95 in ms were 87.039/121.872, 81.341/118.999, and 79.254/115.278. Before dedicated scratch these were 97.241/126.275, 94.972/132.820, and 98.886/134.871. The synchronized coupled-owner suite passed all 15 cases, including gas, grains, heat, bulk and water, and the graphical heated-water return passed. The 30 FPS parent gate remains open.

**2026-11-27T16:25:31Z**

On native 1920x1080 with 608 resident slots, 16 active owners, 12 warmups and 60 samples per band, mean/p95 in ms measured 142.140/203.249 near, 140.944/183.708 middle and 138.454/206.278 far. Before dedicated scratch these were 173.744/218.655, 167.248/212.237 and 175.243/219.521. Every band advanced 2.000000104 s. Post-refactor synchronization validation passed 3/3 native and seam tests with marker correction, 3/3 without correction, all four aperture cases and all ten transport cases. Full-suite verification and the outstanding frame budget remain on the parent.
