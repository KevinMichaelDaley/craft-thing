---
id: ct-lqd8
status: in_progress
deps: [ct-fmj6]
links: []
created: 2026-10-17T03:43:36Z
type: task
priority: 1
tags: [vulkan, performance, physics]
---
# Parallelize deterministic MPM tile compaction and prefix allocation

The current compact_tiles shader uses a single invocation to walk every viewport tile and its neighbors. Isolated3600-tile metadata compaction measures9.626ms on this machine; this explains much of the nominally empty MPM stage cost.

## Acceptance Criteria

Replace the single-invocation viewport scan with GPU-parallel dilation, hierarchical prefix allocation and sorted scatter/copy. Preserve exact9-neighbor halo ordering, empty and clean-cache indirect resets, partial blocks and arbitrary tile counts. No CPU physics labels/readback or resolution/quality cuts. Compiling RED metadata tests cover>65ktiles and GPU budgets .350ms at3600tiles plus10ns per additionaltile. Validate existing mass/cohesion/connectivity suites and actual640x360 presented mean/p95 without loosening existing gates.


## Notes

**2026-10-17T03:48:48Z**

Baseline compaction correctness5/5 passes withvalidation, including>65ktiles sortedhalo andclean-cache resets. Performance RED at3600tiles9.626ms vs unchangednew .350ms stagebudget. New kernel must retain this exact oracle while replacing scalar scan.

**2026-10-17T04:26:06Z**

GREEN: removed scalar compact_tiles; cooperative dilation, hierarchical integer prefix scans, stable scatter/copy, separate logical count and balanced bounded2Ddispatch. CPUlayout3, descriptor2, metadata7 including66049dense tiles, convergence3, winding/mud2, exactcoupledhash and44constraint/image comparisons all pass. Corrected probe synchronization2 feature setup and rebuild dependencies; all targeted logs zeroVUID/SYNChazards. Pinned historical scalar reference with corrected harness:3600empty tiles9.629ms vs parallel0.111ms;66049empty tiles0.422ms. Reference is explicitdiagnostic only; no runtime fallback.24runtimeSPIR-Vmodulesvalidate. Fullheadless/UI/quarter acceptance pending.

**2026-10-17T04:58:42Z**

Full validated headless010ef76 completed exit0:389 reported passes,0failures/VUID/SYNC diagnostics. Log build/diagnostics/parallel-mpm-full-test.log. UI suite running; full quarter functional checks next. Sustained native360 p95 exposed additional ct-tu55 work; compaction exactness remains green.
