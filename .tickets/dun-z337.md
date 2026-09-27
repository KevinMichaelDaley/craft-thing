---
id: dun-z337
status: closed
deps: []
links: [dun-8xhl, dun-2b08, dun-tw2z]
created: 2026-09-27T21:35:05Z
type: task
priority: 2
assignee: kmd
parent: dun-v2vu
---
# Measure native-resolution GPU tick rate

## Acceptance Criteria

Run 1920x1080 simulation grid with resident chunk content, report wall tick throughput and GPU stage times; state active chunk count and limits of result.


## Notes

**2026-09-27T21:37:57Z**

1920x1080 DP-1 on RTX A2000 12GB: 64 resident chunks, 262144 resident cells, 12 ticks after warmup: 276.146 ms/tick = 3.62 Hz; tick+GPU render 278.878 ms = 3.59 Hz; single GPU stage capture rigid 0.742 ms, fluid 268.800 ms, granular 6.076 ms. Sparse resident grid only; full viewport remains separate dun-tw2z.
