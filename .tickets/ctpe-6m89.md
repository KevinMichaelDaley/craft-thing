---
id: ctpe-6m89
status: open
deps: []
links: [ctlo-hjxc]
created: 2026-10-14T03:22:18Z
type: epic
priority: 1
tags: [libresprite, assets, animation, props]
---
# Export multilayer animated LibreSprite props and scenes for the game

Implement design/libresprite_prop_export.md: real LibreSprite script, explicit physics material/role channels, independent color/emission/surface channels, aligned animation package, validator and usable game import. Isolated feat/libresprite-prop-export branch/worktree.

## Acceptance Criteria

Real pinned-host export of authored multilayer animation loads into actual game with exact mixed-material/concave geometry, structural/terrain/rigid roles, persistent appearance and timing/emission. Source document stays intact; invalid exports/imports are atomic; all required tests and coordinated integration pass.

