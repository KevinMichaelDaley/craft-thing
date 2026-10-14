# Extraction optimization and new physics feature ownership

The user split the previous combined extraction job into independent worktrees.
The existing extraction agent owns optimization/parallelization on main. The new
feature worker owns missing fracture/structural behavior and authoring integration
on `feat/structural-fracture-features` in
`/home/kmd/craft-thing-physics-features`, branched from targeted-verified 6e723cd.
Its complete regression/push checkpoint is still the optimizer's responsibility.

Feature epic: `ctpf-rvcy`.

| Ticket | Feature delivery |
| --- | --- |
| ctpf-753m | Proven gap inventory and agreed authoring/provenance ABI |
| ctpf-iueo | Atomic arbitrary-pixel multi-role authoring |
| ctpf-9jiw | Missing release/fracture and source ownership behavior |
| ctpf-5h0n | Persistence of provenance and pending transactions |
| ctpf-ti75 | Combined validation and coordinated integration |

## Optimizer scope

- Preserve current exact component birth, four-connectivity, mixed materials,
  world identity, fair retry and conservative UNKNOWN boundaries.
- Parallelize existing runtime kernels, including existing pixel fracture BFS,
  packing, property reduction and arena relocation. This is algorithm/performance
  replacement, not new fracture behavior owned by the feature worker.
- Finish cold/unchanged/capacity/complexity acceptance, correctness and displays.
- New performance target: actual 1280×720 physics AND display at 30 FPS, confirmed
  explicitly by the user. Measure complete presented-frame wall time against
  33.333ms, GPU stages, cold work and tail latency under stated workloads.
  Upscaling 320×180 physics does not satisfy this target. Keep existing 480×270
  gates intact. Report feasibility honestly and optimize without changing physics
  quality, hiding active bodies, or loosening conservation/contact requirements.

## Feature scope: prove gaps before adding behavior

Structural graph mechanics, sideways/angled supports, delayed failure, basic
material-dependent splitting/cracking/crumble and exact pixel fracture already
exist. Do not create a second implementation or call their parallel rewrite a
new feature. Audit existing acceptance and producer/consumer APIs first; list
precisely which arbitrary-pixel/authoring/release behaviors remain unsupported.
Implement those proven gaps with compiling failing tests.

The currently confirmed new API gap is atomic multi-role asset authoring for the
LibreSprite prop pipeline. Input is a validated canonical material-u16/role-u8/
anchor-u8/object-u32 map, selected explicit state variant, signed64 world origin,
pivot and stable transaction identity. Visual-only playback never replaces mass
or collider geometry every frame.

Rigid-role cells are grouped by four-neighbor connectivity within authored object
identity, preserving all exact mixed-material occupied cells, holes and separate
disconnected components. Material composition determines mass/COM/inertia and
buoyancy; no per-object float/sink or density override. Terrain writes ordinary
material, without an extraction exemption or fabricated root.

Structural-role cells instantiate explicit marked components through the existing
mechanically principled graph. Define a tested representation mapping authored
shape/member/joint metadata to the graph rather than assuming every painted pixel
is already a beam. Use real terrain contacts for roots. A supplied anchor hint
cannot hold a component in midair. Preserve sideways and angled force/moment
transfer, material strength, grace/damage and rigid load support. Missing authoring
metadata must be diagnosed, not silently replaced with a simplified rectangle.

## Atomic ownership and new fracture/release integration

Preflight overlap and reserve body, geometry, graph and renderer appearance
ownership together before any source clear or publication. Failed capacity,
generation or UNKNOWN checks preserve one authoritative prior state. Repeated
transaction IDs cannot duplicate mass or outputs. Coordinate a narrow ABI with
optimizer, renderer and exporter; do not duplicate their allocators.

Keep immutable asset/object/source-texel identity and source appearance frame
distinct from runtime body IDs and storage generations. Birth, structural release
and fracture transfer exact material and appearance/emissive ownership. Define
fragment-local to source-rest mapping so rotations, offset child origins and
mixed-material cuts cannot repaint survivors. Texture normals remain in rest
space until renderer transformation.

Implement missing release/fracture behavior exposed by those new authored inputs:
graph disconnects and material/joint failure must produce correct sound bodies,
fractured bodies or granular crumble using the existing physics mechanisms.
Conserve material volume/mass and linear/angular momentum within documented
tolerances; account for strain/fracture work and dissipation. Capacity saturation
defers a whole transaction, not partial disappearance. Compare equal elapsed time
under different frame cadence, cold support recovery and submerged outcomes.
Existing single-body cuts are not a justification for losing arbitrary authored
object identities or renderer/emission provenance.

All runtime work proportional to cells/components/outputs is GPU-parallel, with
cooperative reductions/scans and transactional allocation. No new serial whole
component solve, packing or arena loop. Optimizer owns shared kernel rewrites;
feature worker extends behavior through agreed hooks and tests, not by overwriting
the optimized shader from its older branch base.

## Persistence, testing and integration

Version and validate new authoring/provenance/graph ownership data. Preserve active
fracture/release countdowns, pending transactions and appearance under rebase,
eviction/reload and save/reopen. Older saves get documented defaults. Renderer
epic ctr-6fii and exporter epic ctpe-6m89 live in separate sibling branches; their
owners remain authoritative for their own ABI/package/animation implementation.

Follow RED-GREEN-REFACTOR. Use analytical/conservation and atomic rollback tests,
arbitrary concave mixed props and explicit structural support cases, plus actual
Vulkan displayed failure/fracture and paused-frame stability. Run merged headless,
UI, quarter/native720 checks and SPIR-V/Vulkan validation before integration.
Coordinate exclusive GPU test windows; CPU work proceeds independently.

Push feature checkpoints to its branch. Do not merge to main, copy stale serial
kernels or close the feature epic before combined tests and parent coordination.
Optimizer performance work remains independent of new-feature completion; report
its 720p feasibility separately from feature correctness.
