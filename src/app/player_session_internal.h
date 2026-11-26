#ifndef DC_APP_PLAYER_SESSION_INTERNAL_H
#define DC_APP_PLAYER_SESSION_INTERNAL_H

#include <stdio.h>

#include "ferrum/player/beaver.h"
#include "ferrum/player/hotbar.h"
#include "ferrum/player/manipulation.h"
#include "player_session.h"

struct dc_player_session {
    fr_input_t input;
    fr_player_registry_t registry;
    fr_player_definition_t definitions[2];
    fr_player_t players[FR_INPUT_PLAYERS];
    fr_player_control_t controls[FR_INPUT_PLAYERS];
    fr_beaver_body_t bodies[FR_INPUT_PLAYERS];
    double values[FR_INPUT_PLAYERS][2];
    uint8_t abilities[FR_INPUT_PLAYERS][2];
    SDL_GameController *controllers[FR_INPUT_DEVICES];
    int64_t device[FR_INPUT_PLAYERS];
    fr_player_drive_gpu_t *drive;
    fr_fire_ray_gpu_t *ray;
    fr_fire_ray_config_t ray_config;
    bool ray_enabled,follow_camera;
    uint32_t spawn_x,spawn_y;
    uint32_t count,fire,ice,frame_offsets[FR_PLAYER_ACTION_COUNT][8];
    fr_beaver_frame_layout_t layouts[FR_PLAYER_ACTION_COUNT][8];
    uint64_t sequence;
    double milliseconds;
    double pending_action_ms;
    uint64_t cast_pressed_at[FR_INPUT_PLAYERS];
    bool presentation_clock;
    bool editor,trace;
    char failure[256];
    fr_player_action_event_t cast_events[FR_INPUT_PLAYERS];
    fr_player_hotbar_t hotbar[FR_INPUT_PLAYERS];
    fr_player_carry_t carry[FR_INPUT_PLAYERS];
    fr_player_manipulation_t manipulation[FR_INPUT_PLAYERS];
    uint32_t split_count;
    fr_player_inventory_t inventory[FR_INPUT_PLAYERS];
    fr_player_chew_t chew_work[FR_INPUT_PLAYERS];
    bool aim_override[FR_INPUT_PLAYERS];
    fr_player_point_t aim_point[FR_INPUT_PLAYERS];
};

static inline bool players_error(char *err,uint32_t cap,const char *text) {
    if(err&&cap) snprintf(err,cap,"%s",text);
    return false;
}


bool dc_player_session_publish(dc_level_view_t *view,char *err,uint32_t cap);
bool dc_player_session_add_keyboard(dc_level_view_t *view,char *err,uint32_t cap);
bool dc_player_session_join_controller(dc_level_view_t *view,int64_t device,bool *backend_failed,
                                       char *err,uint32_t cap);

#endif
