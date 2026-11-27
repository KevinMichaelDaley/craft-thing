---
id: ctowpe-x0yo
status: closed
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


## Notes

**2026-11-27T22:37:06Z**

Confirmed 8,192 mismatches reading particle labels before correction. Staged device-local readback matches 352 restricted and 7,840 open pairs, with zero changed particle labels. All four runtime diagnostic modes passed Vulkan synchronization validation on hidden Xephyr :93. Full aggregate restarted on d105fc10 under ctfm-w80b.
