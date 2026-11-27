#ifndef DUNGEONCRAFT_APP_OFFSCREEN_H
#define DUNGEONCRAFT_APP_OFFSCREEN_H

#include "dungeoncraft/gpu.h"
#include "dungeoncraft/stream.h"

typedef struct dc_offscreen dc_offscreen_t;

/** Create an independent three-by-three workspace, anchored at its first page. */
dc_offscreen_t *dc_offscreen_create(dc_gpu_t *parent, dc_chunk_coord_t origin,
                                    char *err, uint32_t cap);

/** Release a workspace. The owner retains its last published immutable snapshots. */
void dc_offscreen_destroy(dc_offscreen_t *workspace);

/** Admit an immutable streamed snapshot at the expected generation and confirmed epoch. */
bool dc_offscreen_admit(dc_offscreen_t *workspace, const dc_stream_result_t *source,
                        uint64_t generation, char *err, uint32_t cap);

/** Advance toward an absolute world time within a bounded number of solver steps.
 * A failed tick requires rebuilding from the owner's last confirmed snapshot. */
bool dc_offscreen_advance(dc_offscreen_t *workspace, double world_seconds, uint32_t step_budget,
                          char *err, uint32_t cap);

/** Fill an empty result with an owned snapshot for the expected generation.
 * Release with dc_stream_result_release. Failures leave the result untouched. */
bool dc_offscreen_snapshot(dc_offscreen_t *workspace, dc_chunk_coord_t coord,
                           uint64_t generation, dc_stream_result_t *out,
                           char *err, uint32_t cap);

#endif
