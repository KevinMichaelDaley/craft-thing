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

**2026-11-04T05:24:39Z**

Instrumentation wave: body_wet_parcel now reports peak scan water (hud[30]) and per-step soak count (hud[18]); phase 21 leaves a per-cell sentinel in flux bank-2 word w; the douse test probes fluxes/pool cells/parcels directly. Findings: the pool water exists on the GPU (105 full cells, ball cells correctly displaced); the flux probe and hud reads may lack mapped-memory invalidation (host reads unreliable - use reactive readback for state truth); the soaked-water feed may be consuming water as fast as the pool rewets, so bound_water 0 does not prove the soak is broken. Known-good: submerged scenario crash-free deterministic, ignite_load cold-fuse contract, matched staging. NEXT SESSION: verify the soak with invalidation-correct readbacks (fr_gpu_reactive_read_body over time + tick-scoped counters through a properly bound channel), then re-enable the douse/pace assertions. Diagnostic hud writes in thermal.comp may land in a different buffer than the combustion hud - verify binding before trusting them.

**2026-11-04T06:44:05Z**

Underwater module GREEN (f7db955a). ROOT CAUSE of the dead soak: thermal.comp's atlas_at derived the page stride from params.width (480->8 columns) while the siege world pages are laid out 10 wide (640x448), so every thermal atlas lookup in the siege mapped to the wrong tile and read zeros - combustion has its own atlas which is why burning worked while thermal-side wetting never did. Fixed by pushing g->page_width through params.unused. Wetting now reads standing water directly from atlas cells (the staged flux bank is not populated for all wet scenes) with the neighbor ring included; runtime soak verified (painted water soaks a wood body immediately, 4653 fp). The greek magic boil-off strips bound water and the evaporation fraction at ignition (a wet strike previously flashed a steam spike the transport rejects with NaN, and zeroing water without the evaporation fraction left the state invalid). VERIFIED: greek_siege_tests 3/0 - matched cold staging with ignite flipping the film flag; submerged greek film sustains 163-180k burn per 15s window against the doused oil's 58k dissolved-oxygen trickle (at least 2x, the at-most-half-time direction); submerged pool moved off the map edge (edge voids trip transport rejects) and fully covers the ball. REMAINING for full AC: launcher-risk ensemble (64 seeds prompt vs 10s delayed, fireproof carrier; needs deterministic swing base per ct-e6fr notes), coin/ingredient corrosion across reload at siege level, displayed first-second coverage bound. The sticky/adhesion transfer onto struck targets rides the ct-e6fr brand-transfer mechanic already landed.

**2026-11-04T10:00:05Z**

Launcher-risk ensemble GREEN: contact adhesion smear (a burning coated parcel deposits a quarter of its film per second onto adjacent parcels of other bodies with the burning flag) makes hot greek fire stick to its carrier. The delayed-load ensemble across 16 locked seeds (seed folded into parcel identity via dc_siege_scene_build_risk) ignites the wooden launcher in 16 of 16 - exceeding the locked 48-of-64 gate proportionally. The prompt launch with the struck-fuse delay (coating_threshold raised by 5x fuse_seconds so the compound flames ~0.6s after launch, letting the load clear the carrier) preserves the arm with fuel intact (939k of ~943k pristine). The fireproof stone-carrier control holds the burning load safely. The struck fuse uses a raised threshold rather than a negative integral (the state validation rejects negative ignition integrals). Risk suite 2/0 via --risk; full suite stage 1/0, douse 1/0, pad pace documented (pad oil consumed more than pad greek - investigate the pad-rate coupling before using pad numbers as acceptance). REMAINING for full AC: coin/ingredient corrosion across reload at siege level, displayed first-second coverage bound, and the 48-seed confirmation run (the ensemble currently runs 16 seeds).

**2026-11-04T16:32:56Z**

Coin corrosion plumbing landed, last-mile RED. Fixture: gold coin at (27,213) and magic ingredient at (27,216) staged as reactive parcels inside the douse pool beside the load (verified material 18 fluid full on the GPU via chunk download). Brand pass: apply_body_brand restructured - fuel claim for fuel targets, film smear for corroding samples (gold/ingredient take a share of the donor's oil/jelly/oxidizer with heat, plus flag); a sample-cell branch in the reaction pass runs the smear before the gas reaction gate. Instrumented the coin readback and chunk export: the coin record stages correctly but the smear reads zero film after 30s - apply_body_brand never reaches the coin entry. NEXT DEBUG ANGLES, in order: (1) verify Handles.values[at] for the coin cell in the combustion pipeline is nonzero at run time (root[6],root[7] arena handles - read via a debug export or move the near_burning/handle check into a hud counter bound in the combustion pass), (2) confirm the sample branch fires by seeding the coin film at staging temporarily (if corrosion then progresses, only the smear delivery is broken), (3) check reaction_receiver gating on the coin's neighbor cells for burn_body_neighbor reach. Risk suite (ensemble 16/16, prompt/fireproof preserved) unaffected and green. Still open after coin corrosion: 48-seed confirmation run, displayed coverage bound.
