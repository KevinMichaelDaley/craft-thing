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
