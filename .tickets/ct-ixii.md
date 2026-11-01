---
id: ct-ixii
status: open
deps: []
links: [ctlo-r2ap]
created: 2026-11-01T13:32:17Z
type: bug
priority: 2
assignee: kmd
parent: ctlo-d7vm
tags: [gas, mpm, thermal]
---
# Investigate near-zero gas temperature in carved smoke-box cut cells

Native quarter-resolution 1000-frame carved smoke-box gas audit after ctlo-r2ap reports minimum gas temperature 0.04001585 K while the domain begins at 293.15 K and has a finite 650 K source. Pressure remains finite (peak 174.673) and stock/energy drift passes, but the near-zero local transient may disturb buoyancy, ignition or future Greek-fire combustion. Reproduce and trace its local mass, volume, work, heat transfer and MPM coupling without adding an arbitrary temperature clamp.

## Acceptance Criteria

A targeted reference/GPU regression identifies the energy path in the cold cut cell; fix any nonconservative transfer or document a physical adiabatic explanation. Carved smoke-box 1000-frame audit remains finite and displayed frame budget passes twice; make test passes.

