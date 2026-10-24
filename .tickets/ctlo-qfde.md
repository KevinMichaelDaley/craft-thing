---
id: ctlo-qfde
status: closed
deps: []
links: []
created: 2026-10-24T10:32:40Z
type: task
priority: 1
---
# Specialize contact polygon storage to four pixel corners

## Acceptance Criteria

Compiled shader reflection RED shows pure pixel contacts still reserve eight corners. Specialization must retain eight for arbitrary convex geometry and use four for pixel-only contacts. Frozen eight-corner scalar manifold oracle and actual graphical tests remain exact; no geometric approximation or new body limit.


## Notes

**2026-10-24T11:02:11Z**

GREEN: separate typed four-corner pixel module retains the general eight-corner module; strict shader ABI rejects replacing arbitrary convex geometry with pixel-only storage. Compiled legacy/runtime reflection passes; actual core+synchronization validation canonical manifold comparison 7/7 exact, contract 8/8. Real Wayland repeated-air-brush 500-frame run 31.656ms mean/37.566ms worst; four paused captures identical. This specialization does not solve the frame regression.
