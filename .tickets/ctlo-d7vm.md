---
id: ctlo-d7vm
status: open
deps: [ctlo-jjye]
links: [ctlo-ddvz]
created: 2026-10-24T22:01:41Z
type: epic
priority: 1
tags: [fire, smoke, materials, physics]
---
# Fire, heat, smoke, reactive materials and incendiary siege gameplay

Design-first phased feature requested by the user. Ordinary wetness-sensitive fire, heat, oxygen-depleting smoke, MPM-guided continuous non-Newtonian oil, shocked/oxidized material response, fictional Greek fire, persistent damage, dynamic siege lever hinge and three displayed integration scenarios. See design/fire_smoke_reactive_materials.md. Implementation is not authorized in this planning turn.

## Acceptance Criteria

All child milestones and three reproducible integration scenes pass on the actual Vulkan/Wayland machine. Finite fuel/oxygen/energy, correct owner transfers and persistence, no fixed body/fire count cap, 1:1 simulation/render resolution, no hidden physics cadence reductions, complete-frame budget below 30 ms in the locked 480x270 acceptance fixtures. Native 640x360 and future wildlife/lightning bolt systems remain separate.


## Notes

**2026-10-24T22:10:27Z**

Planning completed per explicit Design and tickets first selection. Baseline ctlo-ck1i is the first implementation gate; profiles and dynamic pivot branch independently after it. Ordinary fire requires heat/gas/oxygen; Greek fire adds finite oil/slime, coatings, bound oxidizer and corrosion. Three displayed scenes feed the final 30 ms gate ctlo-jjye. Full lightning propagation ctlo-ddvz is linked but nonblocking. Detailed design and all initial numeric fixture targets are in design/fire_smoke_reactive_materials.md.
