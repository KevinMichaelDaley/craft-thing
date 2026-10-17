---
id: ct-q6fr
status: in_progress
deps: []
links: []
created: 2026-10-17T07:51:26Z
type: task
priority: 1
parent: ct-hhmy
tags: [vulkan, rigid, dynamic-capacity]
---
# Design dynamic GPU body ABI and safe growable resource ownership

## Acceptance Criteria

Audit every fixed64 body assumption; separate runtime body slabs/metadata from structural node/member limits; specify GPU-addressed or equivalent growable storage, generation/ID ownership, resource requests and fence-safe resize/descriptor lifetime. Host allocation coordination is allowed; CPU physics solving or ordinary geometry readback is not.


## Notes

**2026-10-17T08:07:26Z**

Actual IrisXe supports/enables bufferDeviceAddress without shaderInt64. Corrected minimal address-directory RED completes one bounded compute submission (1second fence),90records/2pages at actual544byte body ABI; stub gives90mismatches as expected, zero Vulkan/sync errors. First harness omitted vkEndCommandBuffer and produced VUID00070 plus CPU driver segfault; fixed before meaningful RED, corrected submission completes normally. No device-hang inference. Resource factory3CPU failure/flag tests and feature negotiation4+legacy2CPU tests pass. Runtime storage migration still not implemented.

**2026-10-17T08:10:09Z**

Address-directory GPU GREEN on actual IrisXe completes0.244s:90records/2pages then200/4pages, exact544byte Body stride,0mismatches for IDs/mass/width preservation and priorrecord visit counters. Two1second-fence submissions, zero Vulkan/sync errors, all address resources/accounting released. SPIR-V declares PhysicalStorageBufferAddresses and no Int64. This is a bounded ABI/resource prototype only; gameplay pipeline retains64 until all consumers and transactions migrate. GPU lease explicitly returned to parent renderer suite.
