---
id: ctopp-pmw5
status: in_progress
deps: []
links: []
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
