---
id: ctlo-2q4f
status: in_progress
deps: [ctlo-ldb7]
links: [dun-sndp, dun-i1zd]
created: 2026-10-24T22:06:18Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Persist reactive state across terrain particles rigid pixels and coatings

Implement versioned owner-specific sidecars for thermal energy, fuel/char, water, coating composition, oxidation/corrosion, stable ignition-hazard state and bound oxidizer. Do not steal leaf color or loaded-terrain bits.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Exact state survives extraction, carving, fracture, MPM pool swaps, negative-coordinate streaming, save/reload and owner-generation changes. Atomic admission failure retains all source inventories; old saves preserve existing moisture/colors/ownership and initialize ambient reaction defaults. Corrupt/newer input rolls back. No fixed body/fire cap, per-frame CPU scan or hot-path allocation.


## Notes

**2026-10-25T00:40:18Z**

Progress through fb44d0b: 9 parcel/codec CPU tests, 2 Intel GPU ownership probes, and 4 native Wayland Vulkan ownership cases pass (16 displayed frames at480x270 physics/render with640x448 halo). Cell/shape/MPM handle storage is lazy and dynamically grows; extraction, carve bank staging, both MPM pool modes and leaf-bit handoff preserve state. The ticket remains in progress: impact/analytic paths, particle erase/reuse, GPU runtime admission and actual stream/checkpoint integration remain required. Buffer-address cleanup has passed the four displayed cases; full regression battery is running before its REFACTOR commit. No combustion/smoke milestone or final performance acceptance is claimed.

**2026-10-25T00:51:16Z**

Full regression RED found one new issue beyond the two tracked authored-stage failures: shape_color_resolve_tests reported pool/capacity queries6/6 instead of <=3/3. Fixed callbacks to guard before evaluating arguments and reuse the existing cooperative leader values; the unchanged Intel GPU test now reports exactly3/3,24 copy lanes and0 RGBA errors with core+sync validation. First complete battery had684 C passes and this regression plus the2 known failures. Native validation and full battery are being repeated before the cleanup commit.

**2026-10-25T00:57:58Z**

REFACTOR validation complete: corrected unchanged color-work test reports3 pool queries/3 capacity queries/24 lanes/0 color errors; all4 native Wayland ownership cases pass again. Repeated full make -j8 -i test has685 C passes plus6 fixture checks, only the same2 ctr-ojvy authored-stage failures at67/135, and0 compiler warnings/new failures. State/ownership ticket remains in progress; no final epic frame-time acceptance is claimed.

**2026-10-25T01:12:20Z**

Particle lifecycle GREEN:4 new RED cases reproduced stale state after air/repaint, lost secondary-promotion state, leaf RGB flags becoming soil moisture, and state loss across a negative-coordinate halo transfer. All4 now pass on native Wayland Vulkan at480x270+halo with core/sync validation (16 presented frames); the prior4 ownership cases pass again on Intel. Canonical A erasure retires only its entry and clears stale B handles without retiring unrelated B aliases. Halo runtime variant follows particle handles and promotion; material replacement clears incompatible flags. No ticket/epic closure yet.
