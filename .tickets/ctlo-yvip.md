---
id: ctlo-yvip
status: open
deps: [ctlo-ck1i, ctlo-6syy, ctlo-3hb8, ctlo-h9of]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Validate sticky Greek-fire siege corrosion dousing and launcher risk

Use the same siege mechanism with ordinary-oil and Greek-fire controls, foliage/moving actor proxies, coin/ingredient samples, water jets/submersion and prompt versus delayed launch.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Greek material sticks/melts and continues finite reaction under water; matched fuel depletes in at most half the ordinary-oil time. Converted valuables remain spent rust/ash across reload. Prompt launch preserves launcher; an exposed dry 10-second delayed load ignites it in at least 48 of 64 locked seeds, with fireproof-carrier control remaining safe. First-second visible coverage is bounded using the design's opacity/emission thresholds; no screen-filling source burst or silent inventory loss.


## Notes

**2026-10-28T02:40:35Z**

User order: begin only after the real catapult/constructed-wall burning-coal module, reuse that same supported launcher, then implement the oil/slime coating, finite Greek-fire chemistry/corrosion and water-dousing controls needed by this visible module.

**2026-11-03T13:26:58Z**

Prerequisite status: the shared siege launcher module (src/app/siege_scene.c, ctlo-ao7l) now delivers the pivoted launcher, real latch/pouch release, structural wall and wet control, and the burning-coal shot reaches the locked impact window; the wall-ignition gap and admitted-ball ignition nondeterminism are documented in ctlo-ao7l and ct-e6fr. The Greek-fire module (matched oil/Greek controls, adhesion, dousing, corrosion, delayed-launch risk ensemble) builds on that same launcher and is not started this turn; its reference chemistry exists and is tested (tests/fire/greek_fire_tests.c 5/5, corrosion_tests.c) but the siege-level integration fixtures are not implemented yet.

**2026-11-04T04:15:52Z**

Foundation wave: siege fixture gains dc_siege_load_t (coal/oil/greek) with matched cold coating stock on inert cores, dc_siege_scene_ignite_load (fuse lights at scenario choice; heats to 530K and strips soak water - the strike boil is magical, no transport steam spike), dc_siege_scene_build_douse_pool (stone-rimmed standing pool; edge-adjacent water trips gas transport rejects so the pool sits at x24-40). Coating burn override: mixed greek films consume contact water outright (FR_GREEK_WATER_FEED_*) and roar up to 3x rate with bonus heat while ordinary oil keeps its wet douse; green/magic emission already flagged. Verified crash-free deterministic submerged scenario; submersion pace ratio measured 2.24x (greek/oil consumed). REMAINING, blocking the RED douse/pace assertions: (1) runtime pool soak - body_wet_parcel scans now include the neighbor ring but submerged loads still read zero bound water; the whole repo stages wetness rather than soaking it, so pool-to-parcel soak is unproven pipeline, (2) launcher-risk ensemble (prompt vs 10s delayed across 64 seeds, fireproof-carrier control) - needs the deterministic swing base from ct-e6fr notes, (3) coin corrosion + reload persistence at siege level, (4) displayed coverage bound. The pad-pace dry comparison measured oil 1.73M vs greek 1.65M - investigate why the greek mix does not show its 6x base rate on dry stone before trusting pad comparisons.
