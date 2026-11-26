---
id: ctsc-jisc
status: open
deps: []
links: []
created: 2026-11-26T14:19:25Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-yvip
tags: [fire, coating, vulkan]
---
# Commit body-to-body coating transfer with one writer per parcel

choose_body_target in shaders/reactive/combustion.comp still debits donor film through ordinary writes while neighboring body invocations can atomically credit that same parcel. Reciprocal or multiple contacts have no coherent stock or heat handoff. The separate gold and ingredient transfer now has a phased single-writer design, but this earlier carrier smear uses its own path.

## Design

Build a locked authored multi-body contact replay. Stage transferable oil, jelly, oxidizer and their enthalpy with explicit donor and receiver ownership. Publish the combined transfer once per parcel before combustion and keep the launcher controls.

## Acceptance Criteria

Reciprocal and competing contacts preserve exact coating inventory, finite bound oxidizer and total heat. Delayed, prompt and fireproof launcher controls retain their intended outcomes on the actual GPU fixture.
