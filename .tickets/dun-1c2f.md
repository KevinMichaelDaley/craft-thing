---
id: dun-1c2f
status: closed
deps: []
links: [dun-lj0v]
created: 2026-09-28T02:09:04Z
type: bug
priority: 1
assignee: kmd
parent: dun-vd9a
---
# Stabilize native-resolution free surface at wall-clock speed

The 960x540 spray regression shows that a six-tick fluid step ejects persistent droplets independently of marker correction. Native 1920x1080 still uses that six-tick step to amortize the solver. Profile and implement a stable shorter-step or equivalent GPU fluid integration at native resolution while preserving 60 Hz and no normal-frame copyback.

## Acceptance Criteria

A native-resolution spring-off impact has no persistent spurious high droplets; marker correction and mass conservation remain valid; native view sustains 60 Hz on RTX A2000; interactive and headless suites pass.


## Notes

**2026-10-05T01:24:02Z**

Superseded by the user instruction that native-resolution physics is no longer needed. Quarter-native free-surface correctness and long marker-on/off spring-off checks are completed in ct-vnsh; both recorded zero high airborne water pixels. Closing this native-specific requirement as superseded, not as an implementation of its original RTX/native acceptance.
