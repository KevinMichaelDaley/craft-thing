#ifndef DC_APP_PLAYER_VIEW_H
#define DC_APP_PLAYER_VIEW_H

#include "ferrum/player/input.h"
#include "level.h"

/** Pan the shared world around one actor or a group of split-screen actors. */
void dc_player_view_follow(dc_level_view_t *view, const uint32_t *body_ids, uint32_t count);
/** Center each split viewport on its own actor within the shared visible grid. */
bool dc_player_view_split(dc_level_view_t *view, const uint32_t *body_ids,
                           uint32_t count, char *err, uint32_t cap);
/** Resolve live mouse or stick aim through the selected viewport. */
bool dc_player_view_aim(dc_level_view_t *view, uint32_t viewport, bool controller,
                         const fr_input_state_t *input, fr_player_point_t *target);
/** Present a world-space aim in its own viewport. A null target hides it. */
void dc_player_view_reticle(dc_level_view_t *view, uint32_t viewport,
                             const fr_player_point_t *target);

#endif
