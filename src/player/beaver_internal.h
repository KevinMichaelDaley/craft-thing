#ifndef FERRUM_PLAYER_BEAVER_INTERNAL_H
#define FERRUM_PLAYER_BEAVER_INTERNAL_H

#include "ferrum/player/beaver.h"

typedef struct {
    fr_beaver_region_t region;
    uint8_t color[4];
} beaver_art_pixel_t;

bool beaver_art_source(uint32_t source, beaver_art_pixel_t *out);
bool beaver_art_frame(fr_player_action_t action, uint32_t frame, uint32_t pixel, beaver_art_pixel_t *out,
    uint32_t *source);
void beaver_art_pose(const beaver_art_pixel_t *pixel, fr_player_action_t action,
    uint32_t frame, int32_t *x, int32_t *y);
bool beaver_art_foot_baseline(fr_player_action_t action,uint32_t frame,uint32_t *baseline);

#endif
