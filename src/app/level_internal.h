#ifndef DC_APP_LEVEL_INTERNAL_H
#define DC_APP_LEVEL_INTERNAL_H
#include "camera_offscreen.h"
#include "dungeoncraft/stream.h"
#include "dungeoncraft/world_save.h"
#include "level.h"
#include "player_inventory_save.h"
#include "view_config.h"

typedef struct {
    float seconds;
    uint32_t count;
} dc_level_step_run_t;

struct dc_level_view {
    struct dc_player_session *players;
    dc_gpu_t *gpu;
    dc_streamer_t *stream;
    dc_world_save_t *save;
    struct dc_level_offscreen_handoff *offscreen_handoffs;
    dc_camera_offscreen_t camera_offscreen;
    double world_seconds;
    fr_player_inventory_t saved_inventory[FR_INPUT_PLAYERS];
    dc_chunk_table_t table;
    dc_chunk_coord_t origin,mapped_origin;
    uint32_t camera_offset_x,camera_offset_y;
    int32_t pending_chunk_dx,pending_chunk_dy;
    uint32_t pending_steps;
    dc_level_step_run_t inline_step;
    dc_level_step_run_t *step_runs;
    size_t first_run, end_run, run_capacity;
    char body_path[1024];
    bool marker_overlay,lighting_initialized;
    fr_renderer_mode_t renderer_mode;
    bool spring_enabled,restored;
    bool transfer_pending,transfer_complete;
    dc_gpu_transfer_state_t transfer_state;
    dc_chunk_coord_t transfer_from,transfer_to;
    uint32_t from_x,from_y,to_x,to_y,transfer_amount,transfer_kind;
    uint32_t from_slot,to_slot;
    uint64_t from_generation,to_generation;
    bool destination_bound,smoke_box,stream_ambient_gas;
    bool closing,closing_structure_saved,player_save_staged;
    dc_chunk_colors_t *save_colors;
    bool save_color_stale[DC_GPU_CHUNK_SLOTS];
    uint32_t smoke_closed;
    bool gas_admitted[DC_GPU_CHUNK_SLOTS];
};

/** Test whether a chunk is required by the current camera and its solver halo. */
static inline bool dc_level_chunk_visible(const dc_level_view_t *view, dc_chunk_coord_t coord) {
    return coord.x >= view->origin.x - HALO_CHUNKS &&
           coord.x < view->origin.x + VIEW_CHUNKS_X + HALO_CHUNKS &&
           coord.y >= view->origin.y - HALO_CHUNKS &&
           coord.y < view->origin.y + VIEW_CHUNKS_Y + HALO_CHUNKS;
}

/** Dispose of an unpublished level during failed construction. */
void dc_level_view_discard(dc_level_view_t *view);

/** Keep gameplay world changes private until the player inventory can be saved with them. */
bool dc_level_view_stage_player_save(dc_level_view_t *view, char *err, uint32_t cap);

/** Configure the standard ambient gas model before streaming chunk stock. */
bool dc_level_view_configure_atmosphere(dc_level_view_t *view, char *err, uint32_t cap);
#endif
