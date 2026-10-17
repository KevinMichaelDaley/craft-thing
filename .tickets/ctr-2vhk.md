---
id: ctr-2vhk
status: closed
deps: []
links: []
created: 2026-10-17T21:48:42Z
type: bug
priority: 2
---
# Load authored soil and gravel at full rest density and packing

## Acceptance Criteria

Imported soil/gravel each represent a full cell of rest volume with material-derived mass and full cell packing; retain colors; verify Vulkan density and persistence; launch fresh level03 without destroying previous saved world.


## Notes

**2026-10-17T22:00:12Z**

Confirmed RED imported material mass and CPU seed mass/radius. Fix covers authored placement, generated chunks and brush placement: soil mass1.2, gravel2.7, radius0.5, deformation identity. Existing mixed-pile conservation expectation adjusted for material mass; sand remains unchanged.

**2026-10-17T22:05:49Z**

Latest focused Vulkan import suite passes full cell mass/packing. Global CPU default and GPU brush initialization now match it; old saved world preserved and launcher will use fresh world_chunks_level_piece03_dense. Full suite queued after saved-world tree replay.

**2026-10-17T22:51:24Z**

GREEN: imported, generated and painted soil/gravel use full rest-cell mass (1.2/2.7), identity deformation and radius0.5. Focused chunk3/3, scene import7/7, particle27/27 pass; Vulkan focused run used core/sync validation. Full make test completed; dependency-check regression corrected and Python3/3 rerun. Two baseline authored-stage failures remain ctr-ojvy. Fresh dense demo world preserves prior edited world.

**2026-10-17T23:02:56Z**

Reopened after user reports settling still leaves holes. RED 64-tick boxed slab:384 soil particles collapse into205 cells with179 double-filled. Initialization mass was correct but transfer reserved only slots, not physical rest volume. GREEN volume-aware transfers retain384 soil/gravel cells and zero secondary particles, with exact mass conservation under core/sync validation.

**2026-10-17T23:21:03Z**

Final GREEN: transfers reserve material-derived rest volume, with compression limit1.5 allowing sand sifting but excluding two full-volume soil/gravel grains. 64-tick slabs retain384/384 cells and exact mass, no secondary grains. All27 particle tests pass, including sifting. Actual level03 after90 untouched ticks retains704 wood terrain pixels and zero doubled soil/gravel cells; rendered packing inspected. Full make test completed with only two pre-existing ctr-ojvy failures; final core/sync dense-support3/3 and actual330-tick GUI scene1/1 pass. Previous worlds preserved; launcher uses world_chunks_level_piece03_packed.
