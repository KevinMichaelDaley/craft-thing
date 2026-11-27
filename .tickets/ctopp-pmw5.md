---
id: ctopp-pmw5
status: closed
deps: []
links: [ctvfb-5v03]
created: 2026-11-27T06:05:51Z
type: bug
priority: 2
assignee: kmd
tags: [vulkan, testing]
---
# Skip unbound storage descriptors in Vulkan diagnostic fixtures

## Design

The shared sparse shader fixture emits a storage descriptor write for every declared binding, including unused bulk binding 37 with a null buffer in the hydro-head and hydraulic aperture cases. Keep binding numbers stable while only writing descriptors backed by actual buffers.

## Acceptance Criteria

Run the hydro-head and fluid-aperture probes with synchronization validation and get zero VUID-VkDescriptorBufferInfo-buffer-02998 messages. Preserve sparse binding indices and existing shader results.


## Notes

**2026-11-27T06:05:51Z**

Reproduced nine null-storage-descriptor messages on both the previous paired-owner revision 122c4fbe and the projection refactor. Their six numerical assertions passed on both revisions.

**2026-11-27T06:14:07Z**

RED caught three null bulk writes in the normal hydro-head suite. The fix preserves sparse binding indices, leaves unused descriptors unwritten, and gives every transport or displacement probe that reads bulk a correctly sized zero-stock buffer. All 56 correctness checks passed under synchronization validation after GREEN. The final hydro-head variations and aperture probes pass without validation messages. Optional transport timing remains tracked in ctvfb-5v03.

**2026-11-27T06:42:23Z**

The standalone water-color probe also dispatches shaders that statically read bulk storage. A pre-dispatch RED guard caught all four cases. The fixture now binds a correctly sized zero-stock bulk buffer and all four checks pass with synchronization validation.
