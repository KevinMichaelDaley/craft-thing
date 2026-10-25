#ifndef DC_APP_LEVEL_INTERNAL_H
#define DC_APP_LEVEL_INTERNAL_H
#include "dungeoncraft/stream.h"
#include "level.h"
struct dc_level_view {
    dc_gpu_t *gpu;
    dc_streamer_t *stream;
    dc_chunk_table_t table;
    dc_chunk_coord_t origin,mapped_origin;
    uint32_t camera_offset_x,camera_offset_y;
    int32_t pending_chunk_dx,pending_chunk_dy;
    uint32_t pending_steps;
    char body_path[1024];
    float pending_seconds;
    bool marker_overlay,lighting_initialized;
    fr_renderer_mode_t renderer_mode;
    bool spring_enabled,restored;
    bool transfer_pending,transfer_complete;
    dc_gpu_transfer_state_t transfer_state;
    dc_chunk_coord_t transfer_from,transfer_to;
    uint32_t from_x,from_y,to_x,to_y,transfer_amount,transfer_kind;
    uint32_t from_slot,to_slot;
    uint64_t from_generation,to_generation;
    bool destination_bound,smoke_box;
    uint32_t smoke_closed;
};
#endif
