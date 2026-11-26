#ifndef DC_APP_PLAYER_SESSION_H
#define DC_APP_PLAYER_SESSION_H

#include <SDL.h>

#include "ferrum/player/body_gpu.h"
#include "ferrum/player/chew.h"
#include "ferrum/player/input_sdl.h"
#include "ferrum/player/fire_ray_gpu.h"
#include "level.h"

/** Admit keyboard player and immutable animation assets in a fresh play world. */
bool dc_players_enable(dc_level_view_t *view,char *err,uint32_t cap);
/** Admit at a chosen cell relative to the scene origin and configure automatic
 * camera follow. Authored fixtures can keep their original framing. */
bool dc_players_enable_at(dc_level_view_t *view,uint32_t x,uint32_t y,
                          bool follow_camera,char *err,uint32_t cap);
/** Configure the demo's explicit Q test ability as a real facing fire ray. */
bool dc_players_fire_ray(dc_level_view_t *view,fr_fire_ray_config_t config,char *err,uint32_t cap);
/** Route owned gameplay events, context toggles and controller lifecycle. */
bool dc_players_event(dc_level_view_t *view,const SDL_Event *event);
/** Whether gameplay owns keyboard/mouse, excluding UI/editor contexts. */
bool dc_players_gameplay(const dc_level_view_t *view);
/** Switch to/from UI without retaining latent held controls. */
void dc_players_ui(dc_level_view_t *view,bool active);
/** Accumulate actual presentation time once per displayed update. Action
 * playback consumes it once without replaying fixed physics or button edges. */
void dc_players_frame_time(dc_level_view_t *view,double seconds);
/** Consume one fixed simulation interval before the actual GPU world tick. */
bool dc_players_step(dc_level_view_t *view,float seconds,char *err,uint32_t cap);
/** Reframe the shared streaming window around the active split-screen players. */
void dc_players_prepare_view(dc_level_view_t *view);
/** Apply the current gameplay or editor presentation to the shared view. */
bool dc_players_present(dc_level_view_t *view, char *err, uint32_t cap);
/** Copy action state for diagnostic tests; does not read GPU body state. */
bool dc_players_status(dc_level_view_t *view,uint32_t index,fr_player_t *player,fr_beaver_body_t *body);
/** Join a controller-owned seat (the first seat stays keyboard-owned).
 * Device ids beyond SDL controller instance ids are valid input. */
bool dc_players_join(dc_level_view_t *view, int64_t device, char *err,
                     uint32_t cap);
/** Pin one player's aim at a world point (scripted demos and tests). */
bool dc_players_aim_override(dc_level_view_t *view, uint32_t index,
                             const fr_player_point_t *target, char *err,
                             uint32_t cap);
/** Copy a player's current inventory masses. */
bool dc_players_inventory(dc_level_view_t *view, uint32_t index,
                          fr_player_inventory_t *out);
/** Present one side-by-side viewport per joined player, each following its
 * own beaver. Count 0 restores the single focused presentation. The keyboard
 * and mouse own the first viewport. */
bool dc_players_split_screen(dc_level_view_t *view, uint32_t count,
                             char *err, uint32_t cap);
/** Release controller, input and drive ownership before the level's GPU. */
void dc_players_destroy(dc_level_view_t *view);

/** Write owned material balances into the active world save transaction. */
bool dc_players_save_inventory(dc_level_view_t *view, char *err, uint32_t cap);

#endif
