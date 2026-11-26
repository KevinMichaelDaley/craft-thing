---
id: ctpao-h40q
status: in_progress
deps: []
links: []
created: 2026-11-26T23:12:45Z
type: bug
priority: 2
assignee: kmd
tags: [testing, vulkan, streaming]
---
# Diagnose a full-suite Vulkan context failure in the halo transfer controls

The full make test on 486563d0 reached halo_tests and failed when test_particle_transfer_crosses_chunk_edge_without_duplication created its second 64 by 64 GPU context. The fixture omitted the context error text. Three other halo cases passed.

## Acceptance Criteria

Capture the Vulkan setup diagnostic in the halo fixture. Re-run the exact native test under stable GPU availability and determine whether the failed context is a resource limit or a regression. Then complete make test or report its next concrete failure.


## Notes

**2026-11-26T23:12:45Z**

Exact 486563d0 make test exited 2 at Makefile:871. The same revision has an isolated halo test queued under GPU memory headroom.

**2026-11-26T23:32:27Z**

RED links and drives the real halo fixture with the Vulkan driver deliberately unavailable. It exits with four setup failures, but the assertions suppress the Vulkan error and also print four OK lines. The regression fails on the absent loader diagnostic. The linked fixture additionally enforces one live context for the next native replay.
