#ifndef DC_APP_BONFIRE_H
#define DC_APP_BONFIRE_H
#include "thermal_bench.h"
/** Build loaded, authored dry/wet ordinary-wood piles and nonfuel containment. */
bool dc_bonfire_build(const dc_chunk_coord_t *coords,uint32_t count,
    dc_thermal_bench_chunk_t *out,char *err,uint32_t cap);
#endif
