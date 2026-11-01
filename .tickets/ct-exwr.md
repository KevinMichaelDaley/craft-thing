---
id: ct-exwr
status: in_progress
deps: []
links: []
created: 2026-11-01T19:29:54Z
type: epic
priority: 1
tags: [player, beaver]
---
# Playable material-aware beaver with extensible RPG and local split-screen co-op

User request: player is a beaver wearing a robe and hat. Deliver placeholder sprite art with exactly 6 walk, 8 chewing and 4 casting frames. Support multiple local controller players with split-screen. A/D move, Space jumps, Q casts a fire bolt, E casts an ice storm at a target, chewing trees deposits wood into the responsible player inventory. Every occupied sprite pixel participates in materials: robe/hat can singe, fur can become wet or ignite. Build extensible stat/attribute/ability systems without choosing a ruleset or inventing future abilities. This epic is the next feature priority; prerequisite work starts now.

## Design

Reuse existing C11/Vulkan material, reactive-state, appearance, streaming and prop contracts. First audit integration points and record a staged plan. Test-driven RED/GREEN/REFACTOR with staged commits. Existing unrelated tickets are not automatically required blockers: link only concrete dependencies. No assumed RPG rules, stat names, mana, levels, cooldown systems or extra abilities. Test spell tuning remains data, not a game ruleset.

## Acceptance Criteria

Playable acceptance in actual Vulkan application, not only a separate CPU mock/demo. All child acceptance criteria met. Independent players, target selection, world interaction, inventory, animation and reactive pixel state demonstrated and regression-tested. Placeholder art is sufficient; do not defer required material behavior behind art polish.


## Notes

**2026-11-01T19:30:25Z**

Noninteractive implementation authorized by user. Start with ct-2p38 and proceed through dependency-ready children. Initial main branch pull was already up to date at 99f70261. Baseline full suite launched via ./tools/with-vulkan make test; log /tmp/craft-player-baseline-tests.log. Persistent worker logs under build/agents/beaver-player/.

**2026-11-01T22:33:09Z**

Progress: ct-2p38 closed with RED 67755ace, GREEN e959607c, REFACTOR 081ce58c (10 passing headless cases). ct-xsll native art/source-map substage implemented with RED 429e1dbd and GREEN 4bc14f32 (7 passing cases, plus 10 foundation cases, sanitizer clean). ct-xsll remains in_progress for actual GPU/render/reactive integration. All movement/spell/harvest/co-op/runtime acceptance children remain open; no epic completion claim. Untouched baseline and foundation full rerun both fail the same catapult support assertion tracked ct-lc9o. Existing default test recipe override tracked ct-il37. Details and provisional controls/material coefficients in design/beaver_player.md and assets/player/beaver/manifest.json.

**2026-11-01T22:39:35Z**

Clean pushed implementation checkpoint: code through 9e181e91 is on origin/main after git pull --rebase and git push; git status reported up to date and clean. Commits: ct-2p38 RED 67755ace / GREEN e959607c / REFACTOR 081ce58c; ct-xsll native asset substage RED 429e1dbd / GREEN 4bc14f32 / REFACTOR 9e181e91. Seventeen targeted cases pass; sanitizer runs passed both suites. Original baseline and both full reruns stop at the unchanged catapult support failure tracked ct-lc9o. Default test recipe override tracked ct-il37. ct-xsll remains in_progress, all later runtime children stay open. Next worker should continue live canonical parcel/render binding and actual window acceptance on ct-xsll, accounting for extracted source-rest origins, before movement. Handoff and evidence locations: build/agents/beaver-player/progress.md, design/beaver_player.md, assets/player/beaver/manifest.json, build/beaver_assets/contact_sheet.png. No live beaver animation/reactivity, movement, spells, harvesting or co-op completion is claimed.
