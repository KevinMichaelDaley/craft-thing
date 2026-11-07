---
id: ct-pozx
status: closed
deps: []
links: []
created: 2026-11-06T06:41:16Z
type: task
priority: 2
assignee: kmd
---
# dc_level_view_destroy fails: non-body owner in reactive body checkpoint channel

player_runtime_tests fails at destroy with 'Non-body owner in reactive body checkpoint channel'. Reproduced at 759c708a~1 with session sources reverted, so it is pre-existing from the ABI-8/thermal session wave, not the player manipulation work. Investigate which reactive archive body record carries owner.kind != FR_REACTIVE_OWNER_BODY during level teardown after a fire-ray cast.


## Notes

**2026-11-06T06:41:27Z**

Repro: make build/player_runtime_tests && ./build/player_runtime_tests → FAIL at tests/app/player_runtime_tests.c:86 destroy after the Q-cast walk sequence. Pre-existing (reproduced at HEAD~1). Performance objects now include player/manipulation.c and player/hotbar.c (Makefile PERFORMANCE_OBJ) so display-test targets link.

**2026-11-07T19:34:17Z**

Root cause: ABI-8 grew fr_reactive_state_t to 29 words but shaders/reactive/chunk_state.comp phase-5 body export still strided 34 (6+28), garbling every exported record past the first; the archive validation then saw wrong owner kinds. Fixed to 6u+FR_STATE_WORDS. Same root broke ct-p2ea (checkpoint restore loaded garbled archive so the pile never ignited) and the ct-zaky save failures; both now pass.
