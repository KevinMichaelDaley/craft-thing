#ifndef DC_APP_PLAYER_SESSION_H
#define DC_APP_PLAYER_SESSION_H

#include <SDL.h>

#include "ferrum/player/body_gpu.h"
#include "ferrum/player/input_sdl.h"
#include "level.h"

/** Admit keyboard player and immutable animation assets in a fresh play world. */
bool dc_players_enable(dc_level_view_t *view,char *err,uint32_t cap);
/** Route owned gameplay events, context toggles and controller lifecycle. */
bool dc_players_event(dc_level_view_t *view,const SDL_Event *event);
/** Whether gameplay owns keyboard/mouse, excluding UI/editor contexts. */
bool dc_players_gameplay(const dc_level_view_t *view);
/** Switch to/from UI without retaining latent held controls. */
void dc_players_ui(dc_level_view_t *view,bool active);
/** Consume one fixed simulation interval before the actual GPU world tick. */
bool dc_players_step(dc_level_view_t *view,uint64_t milliseconds,char *err,uint32_t cap);
/** Copy action state for diagnostic tests; does not read GPU body state. */
bool dc_players_status(dc_level_view_t *view,uint32_t index,fr_player_t *player,fr_beaver_body_t *body);
/** Release controller, input and drive ownership before the level's GPU. */
void dc_players_destroy(dc_level_view_t *view);

#endif
