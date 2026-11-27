#ifndef DC_APP_OFFSCREEN_INTERNAL_H
#define DC_APP_OFFSCREEN_INTERNAL_H

#include "level_offscreen.h"

enum { OFFSCREEN_SIDE = 3u, OFFSCREEN_SLOTS = OFFSCREEN_SIDE * OFFSCREEN_SIDE };

struct dc_offscreen {
    dc_gpu_t *gpu;
    dc_chunk_coord_t origin;
    uint64_t generations[OFFSCREEN_SLOTS];
    dc_chunk_time_t time;
    uint32_t held_writes;
    bool failed, water_coupled;
};

typedef struct dc_level_offscreen_handoff {
    struct dc_level_offscreen_handoff *next;
    dc_offscreen_t *workspace;
    dc_chunk_coord_t coord;
    dc_chunk_owner_t owner;
    dc_chunk_time_t source_time;
    uint64_t generation, write_generation;
    double liquid_face_end[4];
    bool admitted, disk_source, liquid_paired, owns_bodies;
} dc_level_offscreen_handoff_t;

/** Resolve a world coordinate to the bounded workspace atlas. */
bool dc_offscreen_page_at(const dc_offscreen_t *workspace, dc_chunk_coord_t coord, uint32_t *slot);
/** Check that the sealed room contains a complete independently movable body group. */
bool dc_offscreen_body_island(dc_gpu_t *gpu, dc_chunk_coord_t coord);
/** Publish a complete body group between two idle owners after checking destination capacity. */
bool dc_offscreen_move_bodies(dc_gpu_t *from, dc_gpu_t *to, dc_chunk_coord_t coord, char *err, uint32_t cap);
/** Borrow the current lease for a coordinate during main-thread work. */
dc_level_offscreen_handoff_t *dc_level_offscreen_lookup(dc_level_view_t *view, dc_chunk_coord_t coord);

#endif
