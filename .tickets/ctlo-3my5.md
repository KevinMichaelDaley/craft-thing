---
id: ctlo-3my5
status: closed
deps: []
links: []
created: 2026-10-24T04:41:07Z
type: bug
priority: 1
---
# Look up body motion without rebuilding the host registry

## Acceptance Criteria

Motion updates find active or retained bodies across dynamic capacity without rebuilding host registry, requiring auxiliary ID storage, or changing cached geometry properties. Existing CPU motion and GPU property tests pass.


## Notes

**2026-10-24T04:45:47Z**

New RED test compiled and fails because lookup rewrites body_count (19 to64), followed by the original null-registry fixture crash. Direct bounded record lookup now passes4/4 CPU cases and3/3 GPU property tests with core/synchronization validation; no auxiliary host registry required.

**2026-11-28T03:55:50Z**

Verified current dc_gpu_find_body_record scans active or retained records to dynamic capacity and leaves registry and cached pixel geometry intact. pixel_motion_cache_tests passed 4/4 again today. pixel_properties_gpu_tests passed 3/3 in the 264-command Xephyr aggregate on the same code.
