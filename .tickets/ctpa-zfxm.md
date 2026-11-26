---
id: ctpa-zfxm
status: in_progress
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

