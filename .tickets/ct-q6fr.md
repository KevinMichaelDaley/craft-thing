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

**2026-10-17T08:16:39Z**

Shared C/GLSL root64/page48 ABI and common typed body/data accessors now GPU verified: 90 then200 records, exact544byte stride, page/root generation roundtrip, zero mismatches and validation errors in0.256s. Runtime remains legacy64; this checkpoint provides one common accessor for subsequent consumers and renderer migration, not gameplay growth.

**2026-10-17T08:18:39Z**

Transactional resource-owner RED compiled and failed2tests, then CPU-mocked GREEN2/2: growth65→200→1025, old store immutable until completed submission, submitted preparations cannot be discarded, and each allocation-stage failure releases pending resources without publication. Body blocks retain addresses; GPU transfer copies old directory prefix and initializes new block; host writes resource-address entries only. Runtime consumer migration and actual GPU owner validation remain pending.

**2026-10-17T08:21:05Z**

Prepared device-local owner verification binary/SPIR-V for90→200→1025 bodies using GPU fill/directory copies and record persistence. Build warning-free and SPIR-V validation passes; GPU run deliberately deferred until parent renderer full-suite/display lease ends. Shared bounded-probe fixture extracted and configurable output size prevents diagnostic buffer overrun. CPU ownership/ABI/feature/allocation/diagnostic contracts15/15 pass; current gameplay remains64.

**2026-10-17T08:22:08Z**

Registered all new GREEN CPU contracts in normal test_gpu_tools. Complete CPU tools regression passes (including shader/dependency contracts and resource tests), log build/diagnostics/body-storage-tools-regression.log. No GPU submissions in this run. Full main GPU suite remains paused and parent renderer suite retains the GPU lease; device-local owner validation remains queued.

**2026-10-17T08:27:14Z**

Required-feature helper RED→GREEN2/2 CPU contracts verifies owned stable pNext chain, precise unsupported errors/output preservation, and enables only synchronization2+BDA (not queried captureReplay/multiDevice extras). Normal engine device creation now calls this helper; gpu.o builds warning-free and regular CPU tools regression passes. Actual engine GPU regression after this feature enablement remains pending lease release; record/shape layout and gameplay fixed64 ABI remain unchanged.
