---
id: ct-f83a
status: open
deps: [ct-q6fr]
links: []
created: 2026-10-17T07:51:26Z
type: task
priority: 1
parent: ct-hhmy
tags: [vulkan, rigid, dynamic-capacity]
---
# Implement parallel dynamic body allocation and actual-count work queues

## Acceptance Criteria

GPU-parallel free/live slot scans, generation-safe IDs, slab/page allocation and indirect counts; host grows real Vulkan resources only at safe transaction boundaries. No fixed compiled body ceiling or full-max work scans. Memory failure preserves pending sources and IDs and retries.


## Notes

**2026-10-17T08:51:05Z**

CPU-only preparatory RED→GREEN3/3 for resource-boundary hierarchical scan planning: no fixed page limit, partial128-lane groups, recursive levels, 64-bit scratch offsets and 2D dispatch within real device limits. UINT32_MAX entries plan5levels (without allocating that GPU payload). This is dispatch/allocation metadata only, not a CPU body scan/solve. Runtime allocator kernels are not implemented yet; ct-q6fr full regression is still running before dependency closure.

**2026-10-17T08:52:46Z**

CPU recording RED→GREEN2/2 prepares hierarchical scan dispatches: ascending block scans followed by descending parent-prefix additions; each pass has compute dependencies and 24byte uvec2-address push constants. Short scratch/alignment/address-overflow checks prevent any GPU commands. This records O(log(page_count)) dispatch metadata only; page/body values are never scanned on CPU. GPU scan shader/test RED remains next; fresh full runtime regression continues independently without source/SPIR-V changes to its kernels.

**2026-10-17T08:54:36Z**

GPU prefix RED observed expected wrong-prefix/totals with no-op shader; minimal GREEN parallel128-lane scan + descending parent addition now exact for1,127,128,129,16384,16385 entries (up to3hierarchy levels), zero mismatches/guard corruption/VUID/SYNC under six1second fences. No serial whole-list scan: each group scans128 pairs in7barrier steps, global dependencies ascend/descend O(log_128 N) passes. Full runtime make92479 paused only after pixel_world8/8 child completed, then resumed immediately after bounded GREEN. Actual allocator reservation/commit remains pending.

**2026-10-17T09:05:03Z**

Atomic allocation RED compiled/SPIR-V validated, executed under bounded Vulkan/sync validation and failed expected tentative ready assertion with stub. GREEN candidate now compiles:64-lane/page count/mask/rank, parallel tentative handles, commit-gated owner writes and separate O(1) status/publication dispatches. NOT GPU-verified yet: parent holds merged renderer regression lease. Main fresh full suite92479 paused only after structural_failure13/13 completed. Remaining allocator scope: actual-count indirect zero-work gates, generation-safe release/reuse, live/ID lists and runtime consumer integration. Current gameplay stilllegacy64.

**2026-10-17T09:06:44Z**

Transactional allocator GPU GREEN verified0.328s with explicit synchronization validation, no errors:90tentative handles with commit0 preserve ownership/free masks; commit1 publishes90once, replay is idempotent;40more defer with required_slots132 and no mutation; store growth128→192 retries same queue and publishes130exact generation1handles. GPU lease returned immediately to parent terrain-placement/demo work. Still prototype only: runtime consumers remainlegacy64, release/reuse/indirect/live-ID work and full integration remain open.

**2026-10-17T09:13:45Z**

Audit identified another growth-dependent hidden bound: legacy ID collision reservation examines only128candidate serials. Once the store exceeds64live bodies,128consecutive occupied authored auto-namespace IDs could cause permanent retries. Design now requires >128collision RED and GPU-parallel stable ID index/allocation (e.g. sorted-gap binary search), preserving serial on failed joint reservations. Prototype currently reserves generation handles only; runtime IDs/live lists remain unimplemented.

**2026-10-17T09:26:10Z**

Removed avoidable CPU loop over new directory pages: CPU RED observed host-visible page upload; candidate now uses only device-local block+directory allocations and48byte host push constants, GPU lane/page builds addresses/free masks, transfer copies old prefix. CPU growth3/3 plus regular CPU tools pass; shader compiles/SPIR-V validates with Shader+PhysicalStorageBufferAddresses only (no Int64 capability). GPU verification pending parent lease: real growth90→200→1025→6000 covers multiple initializer groups, plus64opaque descriptors exercise multi-gigabyte/carry arithmetic without allocating/dereferencing fake blocks. Generation release/reuse RED is also compiled but unrun; no release implementation yet. Main fresh full suite92479 remains stopped after structural_failure13/13 and may require restart if outer timeout expires.

**2026-10-17T09:27:45Z**

Parallel page initialization GPU GREEN confirmed under sync validation: real device-local record growth90→200→1025→6000 across2→4→17→94pages, last growth77pages crosses initializer workgroups; zero record/generation mismatches. Separate64descriptor probe verifies carries and virtual multi-gigabyte plane offsets without allocating/dereferencing fake blocks, zero guard errors. Complete queued build+checks<4s, no VUID/SYNC. No serial CPU page initialization remains; host now emits48byte constants and constant resource calls, GPU lanes build new metadata. Both verified fixtures registered in regular headless recipe. Lease returned to parent UI/demo immediately.

**2026-10-17T09:27:45Z**

Generation release/reuse RED now actually executed: current shader completes bounded release fence but leaves root130/committed0 instead of129/1 for duplicate slot0handle plus invalid sentinel; fails exact release assertion, zero validation errors. Test also covers replay, stale generation after reuse and generations1→2→3, which remain unimplemented next GREEN. No GPU work while parent UI/interactive lease active.

**2026-10-17T09:31:49Z**

Generation-safe release/reuse GPU GREEN verified in 0.457 seconds under explicit synchronization validation: duplicate plus invalid handles release slot 0 exactly once, replay is harmless, stale generation 1 cannot release generation 2, and reuse advances 1 to 2 to 3. All allocation/growth/retry assertions also pass; bounded fences completed without validation errors. Shader phases 5 and 6 use parallel handle ownership CAS plus a separate O(1) count publication. GPU immediately released to parent; no further submissions while parent visual investigation/demo owns it. Production remains legacy 64 bodies; actual-count indirect gates, runtime IDs/live lists and runtime migration are still open. Fresh full main regression remains paused, not reported complete.

**2026-10-17T09:44:18Z**

CPU-only indirect scan recording RED observed 3 pass/1 fail, then GREEN 4/4: all 2L-1 hierarchical passes consume packed GPU-generated dispatch triplets, preserving scan/add order, with compute-write to indirect-command-read synchronization; short, misaligned and overflow ranges reject before any commands. Shared direct/indirect recording avoids duplicate hierarchy logic. No GPU submissions while parent material demo is open. GPU producer/zero-work execution and runtime integration are not implemented or claimed by this CPU contract.

**2026-10-17T09:45:36Z**

Prepared optional bounded GPU work-gate test and no-op stub; both C and SPIR-V compile/validate, but GPU RED has NOT been executed because user material demo owns device. Test covers137 indirect triplets across 2D groups, active→empty→completed→null queue→active transitions, stale output clearing, immutable growth templates and output guards. Stub must remain unimplemented until observed RED under a new explicit GPU lease. Optional fixture is not in normal test recipe yet.

**2026-10-19T02:49:16Z**

Resumed dedicated optimizer worktree. GPU work-gate RED now observed under explicit sync validation: active90-request queue leaves all137 stale indirect triplets untouched with no-op shader, test0pass1fail; all bounded fences complete, no VUID/SYNC. Exclusive lease returned immediately to lighting optimizer before its graphical run. Implementing parallel command producer next; CPU template construction is growth metadata only, GPU queue state controls actual dispatch.

**2026-10-19T02:50:29Z**

Work-gate GPU GREEN now passes all5 active/empty/completed/null/active transitions and137packed triplets under explicit sync validation, zero mismatches/guard damage/VUID/SYNC; process0.19seconds. Each GPU lane writes one complete triplet, immutable growth templates restore active counts and zero triplets suppress stale empty work. Initial GREEN attempt correctly failed after overlooked reserved-word compile error left old stub; corrected identifier and explicitly verified compiler/SPIR-V/C success before passing retry. This is verified queue-control primitive, not yet ordinary-frame integration. GPU immediately released to lighting agent graphical baseline.
