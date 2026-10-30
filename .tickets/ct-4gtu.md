---
id: ct-4gtu
status: in_progress
deps: []
links: []
created: 2026-10-30T09:55:42Z
type: bug
priority: 1
assignee: kmd
---
# Make ordered contact comparison portable across Vulkan GPUs

After the probe accepted NVIDIA, contact_quad_tests reports thousands of bitwise mismatches between optimized and scalar contact geometry on RTX A2000. Characterize contact decisions, feature IDs and geometric error magnitude, then enforce a defensible cross-GPU gate without hiding material contact differences.

## Acceptance Criteria

All six contact case families execute on NVIDIA; overlap decisions and witness feature IDs remain exact or explicitly justified; geometric normal, depth and witness differences stay bounded; dynamic-loop and axis-count gates remain. Full regression advances past contact_quad_tests.


## Notes

**2026-10-30T09:56:28Z**

RTX A2000 contact_quad_tests reports zero overlap-decision mismatches across six 32768-case families, one witness feature mismatch, max normal difference 1.40006924, depth 0.000122070312, witness A 0.854919434 and B 0.999938965. Exact geometry assertion remains RED; this needs tie/feature analysis before tolerances.

**2026-10-30T16:25:02Z**

Full tools/with-vulkan make test on RTX A2000 with current performance changes passed preceding tests and stopped at contact_quad_tests: 32768 quad cases, contact decision mismatches0, feature mismatch1, max normal difference1.40006924, depth0.00012207. This is the same pre-existing NVIDIA exact-geometry divergence; no geometry code changed in the perf branch commits.
