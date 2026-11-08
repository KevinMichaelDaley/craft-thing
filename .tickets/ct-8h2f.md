---
id: ct-8h2f
status: closed
deps: []
links: []
created: 2026-11-02T00:31:51Z
type: feature
priority: 1
assignee: kmd
parent: ct-exwr
---
# Route configurable per-player actions into live beaver gameplay and animations

Device bindings feed independent action states, intents, simulation arbitration and animation events in the actual Vulkan app. No animation viewer or test button.

## Acceptance Criteria

Configurable keyboard/controller bindings; stable exclusive device ownership; press hold release and repeat filtering; focus/disconnect/context clearing; viewport aim ownership; movement and authoritative chew/cast phases synchronized with gameplay; frame-six spell delivery once; actual application admission rendering controls and evidence.


## Notes

**2026-11-02T00:34:45Z**

RED 10b106e8 compiles and fails 5/5 cases. GREEN 55ab2142 passes 5 input/action cases plus 24 existing state/asset cases with -Werror. Configurable per-device actions, ownership, edges/repeat handling, context/focus clearing, per-player target, arbitration and latched frame-six ability event implemented. Full make test running in sustained/input-full-tests.log; SDL and actual runtime integration next; ticket remains in_progress.

**2026-11-02T00:35:58Z**

SDL RED c737af60 compiles and fails 3/3; GREEN f5a835c3 passes 3/3. Controller instance IDs, deadzone axes, keyboard scancodes, mapping-based event consumption and lifecycle clearing share the same router. Full make test completed exit 2 at unchanged catapult support failure ct-lc9o (12 pass/1 fail), log sustained/input-full-tests.log. Actual app integration continues.

**2026-11-02T01:10:35Z**

Live Vulkan play path now exists (429887cc). Core input7/7 and SDL3/3 pass after explicit release rearming across focus/context and snapshots of held controls on device admission/focus gain. Cast returns idle exactly after recovery; no synthetic/demo input exists in --play. Movement retains logical WALK hook but renders only authored idle while user walk art is pending. Actual runtime test passes 60 initial no-input idle ticks, real D movement, focus clearing, intentional Q at frame6, 120 held/no-new-press ticks returning idle, plus pixel comparisons. Fire/ice gameplay effects remain pending; new fire-ray demo ct-9ieb is next priority.

**2026-11-02T02:00:51Z**

Actual focus race reproduced and fixed: SDL global key snapshot during focus-gained included a later queued Q down, blocking that genuine press. Handler now uses ordered events and prior held release blocks. SDL regression4/4, input7/7. User live demo PID40605 received actual Q events and reached frame6; user closed it. Rebuilt80ms PID41100 logs wall11343 cast frame1 elapsed10, wall11775 frame6 elapsed440 event1, wall12059 idle. Presentation sampling (~70ms GPU frames) now drives action elapsed independently of one fixed physical tick, with timestamp clipping for new press. Boundaries399/400,479/480,639/640 pass; skipped emission retained once. Source ASE unchanged; runtime export timing80ms. No user-authored walk yet: live movement shows idle only.

**2026-11-02T15:35:35Z**

Build repair: the GREEN commit wiring player actions made bonfire_display_tests and thermal_bench_display_tests fail to link (undefined dc_players_step/dc_players_destroy). Fixed Makefile rules 2113/2115 to link $(filter-out main.o level.o,$(QUARTER_APP_OBJ)) so the player session stack resolves without duplicating level.c (both test TUs include level.c directly). Both binaries build and bonfire_display_tests passes 0 failures on this machine.

**2026-11-07T20:31:00Z**

Live app integration landed and verified: input_sdl bindings (G/E/F/R + slots 1-8 with edges and repeat filtering), dc_players_step routes aim target, ability arbitration and frame-six cast events into the beaver; player_runtime_tests drives real keyboard movement/jump/cast with runtime-walk-left.bmp and runtime-cast.bmp evidence. Closing per acceptance criteria; split-screen camera join flow continues in ct-1i55.
