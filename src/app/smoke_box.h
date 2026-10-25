#ifndef DC_APP_SMOKE_BOX_H
#define DC_APP_SMOKE_BOX_H
#include "dungeoncraft/color.h"
#include "ferrum/fire/gas.h"
typedef struct {
    dc_chunk_t *chunk;
    dc_chunk_colors_t *colors;
    fr_gas_state_t *gas;
    double *void_fractions;
    bool valid;
    char error[256];
} dc_smoke_box_chunk_t;
/** Rounded explicit gas calibration shared by the immutable scene and runtime. */
fr_gas_model_t dc_smoke_box_model(void);
/** Build independent cold scene chunks in parallel, including finite reservoirs. */
bool dc_smoke_box_build(const dc_chunk_coord_t *coords,uint32_t count,
    dc_smoke_box_chunk_t *out,char *err,uint32_t cap);
/** Release all immutable staging copies after publication or failed preflight. */
void dc_smoke_box_release(dc_smoke_box_chunk_t *chunks,uint32_t count);
#endif
