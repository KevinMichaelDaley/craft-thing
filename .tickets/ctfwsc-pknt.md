---
id: ctfwsc-pknt
status: closed
deps: []
links: [ctlo-ehqj]
created: 2026-11-26T23:38:31Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-ehqj
tags: [testing, forest, physics]
---
# Include the nominal endpoint in the locked forest warmth window

The forest sampler records the accepted floating-point clock at each scheduled ten-second observation. The warmth evaluator compares that accepted time to an exact endpoint, so positive timestep rounding silently skips the planned 120 s sample. Integrated 3c94b591 printed 352.08 K at nominal 120 s but reported a peak of 334.86 K.

## Acceptance Criteria

Select observations by the scheduled sampling deadlines. Verify that nominal 120 s remains included for an accepted clock just after 120 s and that nominal 130 s remains outside the evaluation. Report the true extrema and keep the locked 298.15–318.15 K over 20–120 seconds criterion tracked until physics meets it.


## Notes

**2026-11-26T23:39:56Z**

RED compiles and runs without opening a Vulkan context. The synthetic series uses the same rounded one-sixtieth-second steps as the real fixture. A nominal 130 s sample is ignored, but the old evaluator also drops nominal 120 s and accepts 361 K at that sample.

**2026-11-26T23:41:55Z**

The production fixture and its display variant compile. The synthetic regression now counts the nominal 120 s sample despite the accepted clock drifting slightly beyond 120 and still excludes nominal 130 s. It prints complete minima, maxima and the number outside the locked band. The archived integrated still-air series contains 11 nominal 20–120 s samples with 302.86–352.08 K and six above 318.15 K.
