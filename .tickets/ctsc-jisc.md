---
id: ctsc-jisc
status: closed
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

## Notes

**2026-11-26T14:46:13Z**

The prior shader fails both locked reciprocal and competing contacts because it gives world coordinates to the local receiver lookup. The GPU now resolves contact geometry, snapshots each donor once, admits each combined inbox before any debit, and writes each parcel once. Independent bodies transferred exact Q16 oil, jelly and oxidizer. Maximum parcel heat error was about 1.6e-5 kJ. Identical replays pass after growing the live arena, and arena rollback plus address diagnostics pass. Transfer phases dispatch indirectly with zero work groups when no offer exists. The actual prompt replay opens the pouch at the flight-test angle and preserves all 943728 Q16 arm fuel. The stone carrier survives. The parent 16-seed delayed control passed 16/16 and the updated run is in flight.

**2026-11-26T14:55:46Z**

The targeted Vulkan coating suite passed 6 cases. The clean pool corrosion replay remained gold .009047 and ingredient .565406, the moving-load reload remained .0269 for gold, and submerged dousing retained 45080/0 Q16 late-window consumption. Updated prompt and fireproof controls passed. In the first updated delayed run seeds 0 through 3 had 11, 13, 13 and 9 flaming arm parcels, respectively. Seed 4 aborted during scene construction while other GPU tests were active, with no diagnostic in that test version. The retry prints construction errors and runs after the other siege jobs. The measured per-call transfer share is tracked in ctrc-q7uv.

**2026-11-26T15:05:05Z**

The serialized Vulkan --risk retry completed 2/2 checks. The prompt case opened its actual pouch and retained all 943728 Q16 arm fuel with zero flaming parcels. The fireproof carrier remained active. All 16 existing delayed seeds ignited, with 5 to 15 flaming arm parcels per seed. The parent ticket still tracks the full 64 locked seeds and the application-level criteria.
