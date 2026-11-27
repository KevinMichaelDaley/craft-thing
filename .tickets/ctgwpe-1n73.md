---
id: ctgwpe-1n73
status: in_progress
deps: []
links: [ctfls-xxfd]
created: 2026-11-26T23:57:55Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-ehqj
tags: [forest, gas, diagnostics]
---
# Read forest warmth from a complete authoritative gas probe

The forest warmth check turns an unreadable cell into zero kelvin and derives gas temperature with a duplicate model. Its paired locked and historical comparisons need to distinguish hot gas at ordinary density from spikes associated with void or low local heat capacity.

## Acceptance Criteria

Read the single resident physical probe from the completed authoritative gas packet. Use the live gas model and actual void volume. Preserve the destination and report errors for unavailable or invalid samples. Log local mass, heat capacity, internal energy and pressure alongside the temperature without changing the simulation or locked probe. Exercise mapped fractional-void and failure paths without GPU requirements, and verify the same probe in Vulkan.


## Notes

**2026-11-26T23:59:12Z**

RED builds the actual forest fixture and passes an unbound page to its warmth reader without starting Vulkan. The reader returns 0 K with an empty diagnostic. The regression requires the missing resident owner to be reported.

**2026-11-27T00:07:02Z**

GREEN and refactor pass the mapped-packet test, including swizzled residency, partial cell volume, the live model, compensated energy and existing chunk reads. Unbound, invalid volume, empty stock and device-loss paths keep the output untouched. The three focused gas cases also pass with ASan/UBSan. The forest contract reports unavailable samples and records covered dry-bridge sites as skipped. Still/control samples now publish temperature, density inputs, pressure and work as each observation is accepted. Native warm-probe validation and the matched comparison remain due.
