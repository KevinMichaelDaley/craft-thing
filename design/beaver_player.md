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
cast plays four frames at 100 ms and holds the last frame. The authoritative
`assets/player/beaver/beaver_chomp.ase` supplies eight unchanged 40x40 RGBA
frames at its authored 100 ms timing. One-based frame 1 is idle; 2-4 enter;
5-7 loop during valid held chewing; 8 recovers for 100 ms to idle on release or
target loss. `fr_player_chew_tick` takes the current held intent and freshly
validated wood availability/reach, and advances animation once per simulation
step. It does not mutate wood. Repeated held ticks preserve entry/loop time;
recovery completes before re-entry. Other animation consumers use
`fr_player_advance`; do not advance a player twice in the same step.
If a valid target returns during recovery, complete the recovery and carry any
remaining elapsed time into a new entry. This remains independent of tick size.

`fr_player_drive_gpu_create` binds unique session player identities to actual
body IDs/generations. `fr_player_drive_gpu_submit` dispatches normalized horizontal
input and press-only jumps against GPU-owned bodies. Fixed-tick speed/jump tuning
is explicit and bounded by the existing solver's four-cell velocity contract.
Jump uses the solver's real support flag and consumes that support immediately.
Monotonic 64-bit input sequences suppress retries; stale/missing bodies are
GPU no-ops. The adapter never reads body state to CPU or edits transforms.
Its five Vulkan tests exercise real support/movement, independent players,
sequence retries, invalid batches and stale generations. Application spawning,
SDL routing, fixed-step scheduling and animated material binding still need
integration; this backend alone is not playable acceptance.

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
  or fur, and every four-bit body material ID is assigned. For the placeholder,
  distinct fur/robe/hat/tail regions explicitly alias the existing wettable,
  combustible wood profile; teeth use stone. These reversible provisional
  coefficients preserve supported physical and reactive contracts without
  truncating IDs or widening the body format in an unrelated change. The native
  asset manifest records all aliases. Actual material response still needs live
  simulation and rendering acceptance; an assigned fuel profile is not that proof.
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

## Native placeholder assets

`fr_beaver_asset_build` builds deterministic standard CPRP clips into supplied
cold buffers. `fr_beaver_asset_size` supplies exact capacity requirements.
The standard prop reader validates the completed compiled bytes. No new image
format, runtime image parsing, heap allocation or global mutable sprite state is
introduced. The manifest is `assets/player/beaver/manifest.json`.

`scripts/props/beaver_import.py` uses the existing strict LibreSprite ASE reader.
The original artwork is immutable. The checked-in `.mapping.json` sidecar gives
explicit semantic regions and unique per-frame canonical owners, constrained
to the same material region. Its source hash rejects unreviewed artwork changes.
Newly revealed pixels reserve canonical rest cells; occlusion never releases
their parcels. Fur, cyan hat/robe, and tail are covered; the artwork has no
separately visible tooth region, so no tooth geometry or pixels are invented.
Walk/cast use six/four provisional poses derived from the imported idle pixels.
The generated C header is rebuilt from the ASE and sidecar before asset builds.

The parallel source map retains canonical body/equipment pixel identities across
all poses and facing directions. Zero is transparent; other values are the
40x40 source index plus one. Owners use canonical rest coordinates, independently
of the destination sprite pixel and camera/world position. Occluded source pixels
remain canonical owners even when the current frame does not display them.
`fr_beaver_pixel_owners_trimmed` applies a supplied admission rest origin and
extent to resolve the actual BODY parcel key, while appearance stays canonical.
It rejects out-of-part pixels transactionally. A Vulkan regression extracts a
real wood part, seeds a wet/hot parcel at the cold fixture boundary, resolves its
trimmed owner, moves it through the GPU drive/rigid solver, and verifies the
entire parcel and owner remain unchanged. No per-frame seeding or CPU reaction
solve is involved. This proves the trim binding prerequisite; full pose
deformation, retention of hidden parcels and live exposure still need runtime
integration before ct-xsll closes.
This helper requires admission in canonical rest-canvas coordinates. Cropping
an arbitrary displayed animation frame and subtracting its bounds would bind
the wrong parcels; that path needs the full source-map destination adapter.

Run `./tools/with-vulkan make test_player` for the state and asset checks.
Run `./tools/with-vulkan make build/beaver_atlas` and
`./tools/with-vulkan ./build/beaver_atlas` to export standard `.cprop` files,
little-endian source maps, and a contact sheet in `build/beaver_assets/`.
Rows in `contact_sheet.bmp` are idle, walk, chew and cast; the sheet is an asset review, not an actual
application screenshot. Runtime player animation and persistent reaction
feedback are still unimplemented acceptance criteria on `ct-xsll`.
