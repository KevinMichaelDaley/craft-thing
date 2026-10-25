---
id: ctfs-dmxv
status: open
deps: [ctfs-2hu1]
links: [ctlo-r2ap, ctlo-h9of]
created: 2026-10-25T02:54:23Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, modular, gameplay]
---
# Deliver a visible smoke ventilation box as the next modular system

Build only the gas/oxygen prerequisite needed for the first ordinary terrain bonfire, after the interactive thermal system is delivered. Use a permanent smoke-box scene and injection/vent interaction rather than requiring all future gas/liquid/Greek-fire paths.

## Design

See the modular delivery section of design/fire_smoke_reactive_materials.md. This user-authorized sequence takes precedence over completing broad horizontal foundation tickets first.

## Acceptance Criteria

RED-GREEN-REFACTOR. The placement demo can inject finite colored smoke into an enclosure, open/close a real vent and show buoyant/sideways flow and oxygen concentration. Demonstrate variable-density/expansion behavior with reference-validated pressure/CFL policy; conserve species/energy/pigment and distinguish dilution from oxygen consumption. Walls and chunk edges behave correctly; renderer works with propagated lighting off. Actual displayed Vulkan tests, screenshots, pause/seed/reset checks and full-frame timings are mandatory. No placeholder smoke animation, second authoritative gas solver, silent inventory loss or implementation of submerged Greek-fire coupling yet. Keep broad ctlo-r2ap/ctlo-h9of open for remaining scope; report/push this visible system before combustion.


## Notes

**2026-10-25T02:56:00Z**

Start after the interactive heat/wetting system is runnable, displayed-tested and pushed. Deliver a real smoke/ventilation system before adding combustion; keep future submerged Greek-fire coupling explicitly open in the broader gas ticket.
