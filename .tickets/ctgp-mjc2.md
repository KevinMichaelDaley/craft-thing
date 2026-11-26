---
id: ctgp-mjc2
status: closed
deps: []
links: [ctsg-6060]
created: 2026-11-26T19:10:19Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-r2ap
tags: [gas, vulkan, publication]
---
# Restore fused practical gas publication with a standalone heat stage

The native accepted-publication fixture records three submissions versus its one-submission contract on both 88965f72 and the corrected first-axis path. The test stops at that assertion before checking rejection. Standalone conduction and pressure feedback are included in the observed workflow.

## Design

Trace command batches through conduction, preparation, accepted pressure work and feedback. Preserve temperature refresh, component ownership and recoverability on rejection when reducing completion boundaries.

## Acceptance Criteria

Keep the existing one-submission check for the supported fused route and verify exact accepted and rejected state, clock and policy transitions. Cover standalone heat diffusion as a separate physically accounted path.


## Notes

**2026-11-26T19:23:32Z**

The exact 9513f4f3 Vulkan publication run also reported 40/48 submissions with exact packets for full fused ticks, versus 8/16 expected. The fused geometry comparison fails the same submission gate. Trace the geometry and full-tick count separately while preserving the matching packets.

**2026-11-26T20:12:29Z**

Native Vulkan RED isolated the separate grain feedback command. A warm rotating publication used two submissions with coupled thermal and three with standalone conduction. The real grain fixture produced +0.199835166 horizontal force and preserved the sentinel on rejection, but submitted twice. The coupled fused-geometry fixture measured 16 versus 24 submissions across eight edits with exact packets, one excess submission per accepted interval. Keep its 8/16 gate.

**2026-11-26T20:18:58Z**

Practical direct publication now records grain pressure after the accepted gas commit and guards the dispatch on the GPU rejection word. Refactor checks passed on whole-bank rotation and sparse cell ownership: coupled warm intervals used one submission, standalone conduction used two, accepted horizontal grain force matched RED exactly at +0.199835166, and rejection preserved the sentinel force and gas state. Coupled fused geometry passed the 8/16 submission gate with exact high and low packet words. The sand displacement check passed. Whole physics tick still reports 31/39 submissions and exact packets against its 8/16 gate. The supported coupled full-tick route needs its own ordered input stages.

**2026-11-26T20:22:06Z**

The complete tick fixture now includes coupled thermal/combustion and a real wood parcel with pending shock energy at each terrain edit. Its displayed RED completed all eight intervals in both modes, with exact gas packets, clock, and wood energy at 751.450012, while submitting 16 times in either mode. It retains the 8/16 submission gate.

**2026-11-26T20:29:20Z**

The coupled whole-tick RED used 16 submissions either way. On the completed refactor, the displayed test uses 8 versus 16 submissions with identical packets and clock, consumes actual pending wood shock heat, and preserves 751.450012 of wood energy in both routes. This passed with rotating whole banks and sparse cell owners. The cold route declined fusion before recording any work. Five-second Greek and matched oil traces were identical with the setting off/on. A link-only probe of the final code counted 299 fused and one cold split tick in 300 native Greek calls; disabling fusion produced 300 split ticks with identical physical samples. The dedicated and whole suites are queued.

**2026-11-26T20:41:55Z**

Instrumented e136d9b3 publication prefix reproduced 16/16 coupled submissions with exact packets and heated wood. The preceding presentation trace leaves DC_FRAME_TIMINGS=0 when the option was absent. The gameplay tick treats variable presence as enabled and routes through two traced submissions even though presentation treats zero as disabled.

**2026-11-26T20:48:57Z**

With DC_FRAME_TIMINGS=0 the native test reproduced 16/16 submissions before the change and passed at 8/16 afterward. The untraced paths retain identical gas packets, clocks, and thermal energy in the heated wood. A separate enabled timing test still captured four injected slow logs and excluded them from fence wait time (0.005 ms measured).

**2026-11-26T21:06:48Z**

Exact 486563d0 complete publication suite passed 103 cases with zero failures. It covered one-submission practical publication with rejection, two-submission standalone heat, and the warm combined tick at 8/16 submissions with identical packets and heated wood. The same combined test passes on 3c94b591 with the authored beaver material profiles integrated.
