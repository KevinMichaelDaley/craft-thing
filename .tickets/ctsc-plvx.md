---
id: ctsc-plvx
status: open
deps: []
links: []
created: 2026-11-26T14:26:23Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-yvip
tags: [fire, corrosion, vulkan, tests]
---
# Recover standalone GPU corrosion residue conversion coverage

The existing combustion_gpu_display_tests --corrosion diagnostic fails its first partial-gold-progress assertion at line 251 on d5f030db and on the sustained-contact branch. The separate moving-load and clean persistent-contact cases do show partial gold and ingredient corrosion and preserve checkpoint state. The standalone pre-coated scenario has not reached its full residue and reload assertions.

## Design

Trace which adjacent cell owns the pre-coated gold reaction in the display fixture. Verify fixture geometry and initial parcel, gas, and thermal state before changing either the fixture or runtime.

## Acceptance Criteria

The standalone Vulkan case demonstrates partial gold and ingredient conversion, finite oxidizer debit, complete residue, and stable parcel identity through reload. Body-to-terrain contact and chemistry checks stay green.
