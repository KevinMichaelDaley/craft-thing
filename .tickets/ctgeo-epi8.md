---
id: ctgeo-epi8
status: in_progress
deps: []
links: [ctsg-6060]
created: 2026-11-26T19:23:42Z
type: bug
priority: 2
assignee: kmd
tags: [gas, vulkan, tests]
---
# Restore lazy pressure clear admission in the publication fixture

The 9513f4f3 Vulkan publication suite stops the lazy-clear comparison at gas_multigrid_cases.h line 77. The fixture sees a different backend marker than requested. Establish which pressure path was admitted and whether the configuration or backend routing owns the discrepancy.

## Acceptance Criteria

Restore the existing lazy and eager admission checks and the exact active, quiet, and dormant stock comparison. Retain pressure correctness, dispatch and capability gates.

## Notes

**2026-11-26T19:28:15Z**

The helper currently configures multigrid, fused axis and temporal SOR. Neither sparse RHS nor sparse large-line mode is set by the helper. The production lazy clear requires sparse RHS, a valid existing matrix, and sparse large dispatch for large transport. Compare the fixture with these prerequisites explicitly set before changing production code.

**2026-11-26T19:37:58Z**

Correction from native tracing: sparse RHS and sparse large are enabled by default. With both enabled, the eagerly cleared comparison entered the pressure pass with an invalid matrix on ticks 1–7 and a valid matrix on tick 8. The old fixed check failed on the second lazy tick with 121 prior dirty pressure tiles and an invalid matrix. Checking execution across the full comparison passes at default settings with 53 versus 53 dispatches and exact snapshots. The same rebuilt regression remains RED when sparse options are inherited as zero, failing to keep the quiet mapped snapshot.

**2026-11-26T19:40:24Z**

The corrected displayed fixture passes both with default options and with sparse RHS and sparse large inherited as zero. It enables the two prerequisites locally and restores their prior values. It confirms lazy shader execution during the eight accepted steps, eager shader exclusion, 53 versus 53 dispatches, and bit-identical active, quiet, and dormant snapshots. The full displayed publication and whole-project suites are pending.
