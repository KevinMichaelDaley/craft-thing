---
id: ctowpe-1es0
status: closed
deps: []
links: []
created: 2026-11-27T18:56:40Z
type: bug
priority: 1
assignee: kmd
parent: ctfm-w80b
---
# Track GPU shader and material dependencies in both camera benchmark builds

The aggregate suite and a fresh run of tests/vulkan/gpu_dependency_tests.py fail four checks because both offscreen camera benchmark executables compile the native runtime directly without its private GPU, carve shader, coating and generated material prerequisites.

## Acceptance Criteria

Both camera benchmark targets use the shared GPU source dependencies and rebuild when their compile configuration changes. The GPU dependency tests pass.


## Notes

**2026-11-27T18:57:16Z**

GPU dependency suite passed 10/10 after reproducing four existing failures and both new configuration assertions. A make dry run with a changed coating layout schedules both benchmark compiler invocations.
