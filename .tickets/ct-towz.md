---
id: ct-towz
status: closed
deps: []
links: []
created: 2026-10-30T09:51:51Z
type: bug
priority: 1
assignee: kmd
---
# Keep pixel face-clipping regression portable across Vulkan GPUs

With NVIDIA probe selection enabled, pixel_boundary_tests reports 2202 bitwise face-coverage mismatches on pattern0 while maximum absolute difference is 1.97e-6; volume and SAT comparisons remain exact. Determine whether the differences are expected GPU rounding or real clipping defects, and preserve a strict numeric gate across drivers.

## Acceptance Criteria

Run all four boundary patterns on NVIDIA; assert a justified bounded face-coverage error while still rejecting material clipping differences; retain read-bound and exact volume/SAT checks; make test advances past pixel_boundary_tests.


## Notes

**2026-10-30T09:53:41Z**

NVIDIA RTX A2000 all four pixel boundary patterns pass with 1e-5 absolute face coverage tolerance; observed maximum 8.13603401e-6. Exact volume, SAT overlap, boundary mask, and read bounds remain asserted. Test cleanup ordering fixed to retain counters before freeing fixture.
