---
id: ctpa-zfxm
status: closed
deps: []
links: []
created: 2026-11-26T10:27:13Z
type: bug
priority: 1
assignee: kmd
parent: ct-xsll
tags: [player, beaver, fire, save]
---
# Run and persist combustion in the normal play sandbox

The normal --play entry point enables the thermal module without an atmosphere or combustion. A hot player fur parcel cannot consume fuel or produce char on normal world ticks. Load ambient air for fresh and older chunks and restore saved gas through the standard streamed checkpoint path.

## Acceptance Criteria

A real --play world reacts on accepted physical ticks. A heated canonical fur parcel consumes fuel and produces char, and its parcel and the affected gas checkpoint survive a graceful save/reload. Legacy gasless play saves remain loadable. Existing authored clearing, fire demo and gas checkpoint regressions pass.


## Notes

**2026-11-26T11:06:23Z**

RED fd724555 on the RTX A2000 reproduced unchanged fur fuel after 24 accepted play ticks and missing gas stock on a legacy reload. GREEN 06132cf5 enables the shared ambient model and combustion. With a fixed ignition draw in the test fixture, normal GPU ticks consumed fur fuel and produced char. The final sandbox rerun passed 3/3, including exact chunk gas and parcel round trips, legacy migration, and the normal --play authored clearing. The sampled fur went from 39,322 to 39,219 fuel units and gained 20 char units. The fire-ray demo passed 1/1 with an exact gas reload. CPU/player/build and GPU dependency checks passed. The smoke-box display test and frame tool now link the normal level and player objects.

**2026-11-26T11:18:16Z**

The timed bonfire gate failed identically on pinned main b800354f and this branch at dt <= 0.25 s under simultaneous Vulkan work. That comparison is tracked in ctpa-up5f. The smoke-box save/restore and streaming passed. The recovered roof-vent case has independent pressure and plume follow-up under ctpa-hxez and ctpa-mv8b.
