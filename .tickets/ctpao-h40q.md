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

**2026-11-26T23:36:34Z**

The controlled loader failure now reports the actual Vulkan error on every failed case, with no false OK lines. Scalar and particle fixtures release the split-grid context before opening the reference grid and keep the downloaded endpoints for the same parity checks. Assertion exits release their active context. The executable enforces the single-live-context budget. The diagnostic, host player, tool, recipe and shader checks passed. The historical blind failure still requires the stable-memory NVIDIA recheck.

**2026-11-26T23:51:52Z**

The guarded NVIDIA RED reproduced overlapping-context failure. The fixed NVIDIA fixture completed all four transfers under the same one-context budget. A separate llvmpipe run also passed all four through a test-only device-selection override. The exact historical, unguarded 486563d0 replay remains queued and the original failure cause is still unconfirmed.

**2026-11-27T00:13:08Z**

A separate pinned integrated run at 3c94b591 exited 2 on the later large-body carve timing gate. It preserved exact pixel and mass results. The original unguarded 486563d0 halo replay is still pending under the GPU headroom gate.

**2026-11-28T04:15:50Z**

Recovered the exit receipt and log for the exact unguarded 486563d0 replay. It passed all four cases on the RTX A2000, including the second context. The refactored full suite passed all four as well. The original failing executable omitted the Vulkan result, so the precise resource rejected during that original run cannot be recovered from this log.

**2026-11-28T06:26:18Z**

Checked historical kernel records, the exact full-run log, and its fixture timestamps. On Nov 26 at 15:06:51 PST NVIDIA reported NV_ERR_NO_MEMORY allocating DMA mapping VA. The earlier world_body_tests created build/world_bodies_Z5p7im at 15:08:05; the halo second-context failure occurred after world_body_tests and gpu_tests, before the full run exited at 15:09:30. No NVIDIA diagnostic in that later interval identifies the failing halo call. The earlier kernel event establishes mapping pressure on the host, but cannot be attributed to this context. Current mapped-buffer startup also collapses vkBindBufferMemory and vkMapMemory into the same message.
