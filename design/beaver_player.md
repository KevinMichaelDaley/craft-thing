# Beaver player integration

Epic `ct-exwr`; implementation order: `ct-2p38`, `ct-xsll`, `ct-ortp`,
`ct-hk3v`, `ct-unkh`, `ct-1i55`, then runtime acceptance `ct-vkik`.
This document describes interfaces and provisional decisions, not playable
acceptance evidence.

## Ownership and rules

The session owns an immutable definition registry and independent player
instances. Definitions have namespaced keys and typed identifiers for attributes,
stats and abilities. Register definitions before sealing the registry and
constructing players. The registry and each player's value/ability buffers are
caller-owned; no frame allocation or global mutable player is needed. Numeric
identifiers are registry-local, not save-file identifiers; future serialization
must store keys and validate them against the loaded registry. Player identities
are nonzero session identities assigned uniquely by the session, never controller
device indices or reusable Vulkan body slots.

No built-in stat names, levels, mana, damage formula, cooldown or progression
rules are assumed. Test abilities and their tuning will be explicit data.
Movement, inventory and GPU pixel owners belong to the responsible player;
sharing immutable assets does not authorize sharing mutable reaction state.

Targets and player positions use signed 64-bit chunk coordinates and finite
local coordinates in [0,64), avoiding global floating-point position precision
loss. Viewport-to-world conversion belongs to the owning camera.

## Actions and provisional bindings

Animation uses integer elapsed milliseconds. Walk loops six frames at 100 ms;
chew plays eight frames at 80 ms; cast plays four frames at 100 ms. Chew/cast
hold their final frame on completion until the consumer transitions or explicitly
restarts. Setting the same action does not implicitly restart it. These timings
are placeholder tuning, not a ruleset. Every requested frame must remain visible.

Keyboard: A/D movement, Space press jump, Q press fire bolt, E press targeted ice
storm. C held is the provisional chew input. Keyboard targeting uses the mouse
world location in that player's viewport. Controller proposal: left stick moves,
south face button jumps, west fires, north casts ice, east holds chew, right stick
aims a bounded world target. Join/disconnect/reconnect semantics belong to
`ct-1i55`; these bindings do not claim input is already implemented. Key repeats
must not duplicate press actions. Disconnect/focus loss must clear intents.

## Existing contracts and concrete gaps

- `fr_prop_asset_open` consumes compiled CPRP assets without allocations.
  Physical material and appearance channels remain separate. Per-frame changing
  physical metadata requires explicit state-variant policy; replacing world
  matter on every animation frame is forbidden.
- `fr_appearance_owner_t` uses immutable identity/generation and rest coordinates.
  Animation needs an explicit frame-pixel to canonical body/equipment pixel map,
  preserved under flips and translation. Transparent pixels have no material.
- `fr_reactive_state_t` and GPU generation-bearing parcel handles are the existing
  source of fuel, water, heat and ignition history. Animation must reference
  them, not recreate ambient state or maintain a cosmetic parallel solver.
- `fr_gpu_place_terrain_prop` places terrain. `fr_gpu_pose_authored_rigid` is a
  cold import helper. Neither is a gameplay animated-player API. Add the runtime
  adapter against actual GPU body/reactive/render storage with tests before
  claiming a visible reactive player.
- The current physical registry has wood and other world materials but no cloth
  or fur. Distinct art regions and stable bindings alone do not establish cloth
  singeing or fur wetting/ignition; material support must be implemented and
  validated through the shared simulation.
- `dc_level_view_t` owns one GPU world and one camera today. Split-screen must
  render multiple cameras over that same stepped world, not create independently
  simulated level views. The current editor also consumes A/D through WASD pan;
  gameplay mode must route those controls explicitly.
- Thermal probes and diagnostic readbacks are not runtime spell effects. Fire
  and ice need bounded GPU commands on real material/reaction state. Chew needs
  a committed removal result before inventory credit, including concurrent
  players and missing-page retries.

## Validation boundary

Each implementation stage uses compiling failing tests, a RED commit, minimal
GREEN implementation and commit, then cleanup and full `./tools/with-vulkan make
test`. The original baseline started at 2026-11-01 19:29:04 UTC as make PID 5759
under shell 5758, logging `/tmp/craft-player-baseline-tests.log`. Code/build work
waits for that identified process to exit. No baseline pass is claimed before
its result is observed.

CPU ownership/asset tests are prerequisite evidence only. Rendered Vulkan
acceptance must show movement and collision, all 6/8/4 frames, persistent local
material response, real tree removal/inventory conservation, material-changing
spells and independent simultaneous local players in split-screen. Child tickets
remain open until their actual criteria are met; the epic stays open until all
children and real application acceptance pass.
