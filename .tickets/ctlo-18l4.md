---
id: ctlo-18l4
status: closed
deps: [ctlo-2q4f]
links: [ctlo-2q4f, ctfs-2hu1]
created: 2026-10-24T22:06:18Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Transport heat wetness and phase-change energy conservatively

Implement thermal storage, conduction, contact/convection/radiation exchange, fuel drying, water wetting and evaporation with explicit environmental sinks. Use accepted simulated time rather than renders or rigid substeps.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Reference and Vulkan tests close energy and water/vapor ledgers, preserve nonnegative state, show finite penetration and heat exposure, and prevent ordinary ignition while a maintained wet control remains wet. Equal-time cadence, pause, body contacts and chunk seams agree within locked tolerances. Water cooling never deletes fuel or grants free heat; exchange work is parallel and localized.


## Notes

**2026-11-01T01:37:15Z**

2026-10-31: RED compile-and-fail thermal_phase_tests reproduced absent bound-water-to-vapor and latent energy exchange. GREEN CPU reference now transfers finite Q16 bound water to FR_GAS_VAPOR with gas sensible energy plus vapor reference potential, preserving combined energy; 2/2 new and 14/14 existing thermal tests pass. GPU kinetic flux, fractional progress, checkpoint/cadence and full acceptance remain in progress.

**2026-11-01T01:55:44Z**

2026-10-31: CPU phase reference, ABI6 fractional drying persistence/split/migration, and terrain/BODY GPU evaporation are implemented. Focused CPU thermal phase5/5, precision7/7, reactive checkpoint2/2, GPU drying5/5 at480x270 (including rotated BODY, humidity, equal-time cadence and chunk seam) pass. GPU water loss76 Q16 had combined energy drift -1.32e-6 kJ. Existing BODY heat budget tests had enabled open-sky radiation while asserting a closed contact ledger; baseline pre-drying shader reproduced the two failures. Corrected contact-only fixture now passes9/9 with zero measured drift. Unrelated transported-flame roof test remains magenta on baseline shader and is tracked as ct-5pas.

**2026-11-01T02:02:23Z**

Final regression: tools/with-vulkan make -j8 test passed including 6/6 phase GPU cases; rigid thermal display suite 17/17; CPU phase 5/5, precision 7/7, checkpoint 2/2. Maintained hot wet wood retained fuel while dry control burned. Wetness-to-vapor and heat ledgers, cadence, body rotation, chunk seam, ABI migration and checkpoint are covered. Roof display baseline bug tracked separately as ct-5pas.
