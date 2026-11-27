---
id: ctvfb-5v03
status: open
deps: []
links: [ctopp-pmw5]
created: 2026-11-27T06:13:11Z
type: task
priority: 2
assignee: kmd
parent: ct-79mx
tags: [vulkan, testing, performance]
---
# Measure no-transfer Vulkan timing with valid bulk storage

## Design

fluid_transport_work_tests benchmarks true zero-stock bulk buffers after ctopp-pmw5. Its duration ratios vary under competing GPU workloads. Use a dedicated RTX window and matched revisions with valid descriptors to distinguish scheduling noise from transport work.

## Acceptance Criteria

Run repeated zero-speed, full-recipient, long-travel, and sub-unit-travel comparisons on an otherwise idle device. Keep the current bounds and numerical controls, and diagnose a sustained miss before changing any limits.


## Notes

**2026-11-27T06:13:11Z**

With RTX utilization near 99 percent, the predecessor failed one of three validated timing runs. The corrected fixture failed its initial combined timing run and three validated repeat runs, with overruns moving among modes; with validation disabled one of two runs passed. The fixture changes descriptors and tests only. Exact transport results passed.
