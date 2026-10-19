---
id: ctr-12pa
status: closed
deps: []
links: [ctr-9e4n]
created: 2026-10-18T02:32:21Z
type: bug
priority: 1
---
# Exclude loose particles from terrain component extraction and restoration

## Acceptance Criteria

Tagged terrain grains join a detached mixed body; untagged dirt and leaf debris touching it remain particles, do not bridge terrain components, and remain untagged across save/load and prop bond restoration.


## Notes

**2026-10-18T02:44:56Z**

Implemented shared terrain component membership for local union, cross-chunk union and historical bonds: tagged solid terrain remains material-independent, while untagged granular dirt/leaf bits stay outside the body and retain their particles. Brush edits of loose grains do not release unchanged terrain. Prop restoration never reapplies a cleared terrain role. MPM material changes clear obsolete cell flags, preventing loose grains from inheriting vacated terrain tags. RED reproduced 13 versus 11 body pixels, prop re-tagging, and stale flags after transport. GREEN focused core/sync validation: loose role 2/2, prop restoration/persistence 7/7, mixed component/equilibrium 3/3. Full make test completed with only the two pre-existing authored_stage_tests failures in ctr-ojvy; actual level03 scene 1/1 with core/sync validation and no VUIDs or hazards. Logs build/loose_component_full_suite.log and build/loose_component_scene.log. Updated design and extraction roadmap. Previous paused demo PID exited during checks; reopening its saved world without resetting it.
