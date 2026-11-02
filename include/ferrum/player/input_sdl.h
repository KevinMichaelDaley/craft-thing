#ifndef FERRUM_PLAYER_INPUT_SDL_H
#define FERRUM_PLAYER_INPUT_SDL_H

#include <SDL.h>

#include "ferrum/player/input.h"

#define FR_INPUT_KEYBOARD_DEVICE INT64_C(-1)
#define FR_INPUT_SDL_AXIS UINT32_C(0x80000000)

/** Install documented defaults on a connected device. Keyboard uses SDL
 * scancodes; controllers use buttons and FR_INPUT_SDL_AXIS|axis. Callers may
 * replace mappings through fr_input_bind; this adapter never chooses actions. */
bool fr_input_sdl_defaults(fr_input_t *input, int64_t device, bool controller);
/** Translate SDL events using instance IDs, never enumeration indices.
 * Return true only for bound controls owned by a gameplay player. Focus and
 * disconnect notifications clear state even when returning false. Opening,
 * closing, and assigning controllers belong to the application's join flow. */
bool fr_input_sdl_event(fr_input_t *input, const SDL_Event *event);

#endif
