#ifndef DC_APP_WATER_CHECKPOINT_H
#define DC_APP_WATER_CHECKPOINT_H

#include "dungeoncraft/liquid_seam.h"
#include "level.h"

/** Recover the saved camera mapping and durable lease before loading its chunks. */
bool dc_level_restore_water_camera(dc_level_view_t *view, char *err, uint32_t cap);

/** Stage a confirmed continuation before releasing the owners for their final saves. */
bool dc_level_stage_water_checkpoint(dc_level_view_t *view, char *err, uint32_t cap);

/** Restore the continuation once its saved world tiles are mapped again. */
bool dc_level_resume_water_checkpoint(dc_level_view_t *view, const dc_liquid_seam_endpoint_t *tiles,
    uint32_t count, char *err, uint32_t cap);

#endif
