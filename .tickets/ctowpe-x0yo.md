---
id: ctowpe-x0yo
status: in_progress
deps: []
links: []
created: 2026-11-27T22:26:04Z
type: task
priority: 1
assignee: kmd
parent: ctfm-w80b
---
# Read runtime aperture diagnostics from the native transport buffer

The aggregate displacement-runtime diagnostic compared all 8192 face prefix/suffix pairs against unrelated MPM label storage after native transport moved to device-local buffers.

## Acceptance Criteria

Read the actual native aperture prefix and suffix pairs back from the GPU, compare every cell against rigid face coverage, retain displacement and transport dispatch checks, and demonstrate the particle label buffer remains independent.

