#ifndef DC_APP_THERMAL_BENCH_H
#define DC_APP_THERMAL_BENCH_H
#include "dungeoncraft/color.h"
#include "ferrum/fire/reactive_chunk.h"
typedef struct {
    dc_chunk_t *chunk;
    dc_chunk_colors_t *colors;
    fr_reactive_chunk_t reactive;
    char error[256];
    bool valid;
} dc_thermal_bench_chunk_t;
/** Cold immutable fixture construction, parallel across independent chunks. */
bool dc_thermal_bench_build(const dc_chunk_coord_t *coords,uint32_t count,
    dc_thermal_bench_chunk_t *out,char *err,uint32_t cap);
/** Release all programmatic fixture copies after GPU publication. */
void dc_thermal_bench_release(dc_thermal_bench_chunk_t *chunks,uint32_t count);
#endif
