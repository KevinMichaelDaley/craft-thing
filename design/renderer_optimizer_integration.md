# Optimizer checkpoint integration into the renderer

The requested checkpoint merge combines renderer `fb395f4` with committed main
`7b65fd5`. It deliberately excludes the optimizer's subsequent diagnostic and
dynamic rigid-body pool work. The branch remains `feat/material-light-renderer`;
this does not merge the renderer back into main.

## Preserved paths

- Lit and textured normal rendering uses `fr_renderer_record` when enabled.
- Reference mode and all physics/debug overlays use `dc_gpu_record_draw`, retaining
  the GPU-selected typed/legacy fallbacks and exact image/coverage behavior.
- Paint/setup commands retain their full simulation-grid dispatch dimensions.
- Presentation retains the optimizer's acquired-image transfer-stage dependency
  and final present transition scopes, mailbox/finite acquire and per-image
  semaphore ownership.
- Parallel extraction/properties, multi-workgroup buoyancy, batched contacts,
  cooperative fluid transport/displacement and hierarchical MPM compaction are
  merged with their shader layouts and private C bookkeeping together.

Rebuild the whole merged worktree before running GPU consumers. Contact work ABI
is version 2 with typed dispatch at activity word 15; stale shader overrides and
old direct-source binaries are not compatible. The merged build guards now cover
the material-cut test and renderer direct-source visual/performance binaries.
The scene integration test is included in `make test` so both rendering routes
remain covered by the regular battery.

## Scope and outstanding work

This merge does not claim completion of renderer performance, authored appearance
transfer/persistence or final `ctr-qlsd` integration. Likewise it does not fix the
known extraction body-count ceiling or cached-world dynamics gating: those are
active optimizer/physics tasks, and the imported `pixel_residency_tests` retain
their intended RED cases outside the ordinary passing regression battery.

At this checkpoint the physical body/shape format remains the optimizer's existing
format; future dynamic storage must coordinate the renderer's buffer bindings and
source-texel ownership rather than transplant old fixed-size accessors. Real world
data remains authoritative independently of camera visibility.

Validation is recorded in ticket `ctr-8m5w`, including exact merged commit IDs,
headless/synchronization results, displayed renderer checks and any still-failing
feature or performance acceptance. Logs and captures stay under this worktree's
ignored `build/` directory. Repository tool setup is `make tools`, followed by
`./tools/with-vulkan ...`; no temporary SDK paths are required.

The merged full `make test` passed with core and synchronization validation;
`make test_ui` and the separately validated renderer scene/visual tests passed.
All 28 built SPIR-V modules validated. The isolated, validation-disabled renderer
budget test remains RED: at 480x270 with sun and four point lights, added mean GPU
time was 10.563 ms against the unchanged 2 ms target (lighting 7.030 ms, optical
2.350 ms). This is tracked by `ctr-21eh`, not waived by this integration.
