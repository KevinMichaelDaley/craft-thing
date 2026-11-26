---
id: ctwi-7ukj
status: closed
deps: []
links: []
created: 2026-11-26T17:47:20Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, input, sdl]
---
# Route gameplay and editor input through the owning SDL window

The live loop forwards SDL focus and keyboard events from every window to its current view and samples the process-wide mouse state without verifying which window receives it. In a multi-window process a foreign focus event can rearm or clear the play session, a foreign keyboard event can toggle editor mode, and a foreign pointer can provide game aim coordinates.

## Acceptance Criteria

Two live windows prove that the foreign window cannot restore or clear gameplay focus, steer the player, toggle editor mode, or supply pointer aim. Owned input and controller controls work after the view regains focus. Editor input and scripted events retain their normal behavior.


## Notes

**2026-11-26T17:49:04Z**

On the unmodified runtime, a second live SDL window rearmed controller aiming after the game lost focus. Its actual mouse position also supplied target coordinates to the keyboard player in the left viewport. Both regressions compiled and failed on the X11 Vulkan fixture.

**2026-11-26T17:57:18Z**

The native app and the player session now filter events using the view’s SDL window ID, and pointer and held-key polling require ownership of the corresponding focus. Both two-window regressions pass with the covering window present. All ten split-screen and six player-action cases passed, along with twelve native input and SDL routing checks. The actual quarter-native app passed its scripted seed entry and reset smoke test. Owned editor text, controller ownership and the addressed and unaddressed script paths are covered.
