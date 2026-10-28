#ifndef DC_APP_BONFIRE_H
#define DC_APP_BONFIRE_H
#include "thermal_bench.h"
#include "dungeoncraft/bonfire_layout.h"
#include "ferrum/structural/gpu.h"
enum {DC_BONFIRE_KINDLING,DC_BONFIRE_STARTER,DC_BONFIRE_LOG};
#define DC_BONFIRE_STARTER_K 1800.f
typedef struct {uint32_t object,kind,x,y,width,height;bool wet;float temperature_K;} dc_bonfire_piece_t;
/** Immutable fixture description, not a limit on runtime rigid-body admission. */
typedef struct {fr_structural_gpu_graph_t pit;dc_bonfire_piece_t pieces[2u*DC_BONFIRE_PIECE_COUNT];uint32_t count;} dc_bonfire_plan_t;
/** Build two constructed pits and explicitly separate rigid fuel objects. */
bool dc_bonfire_plan(dc_bonfire_plan_t *out);
/** Place real anchored construction and admit the staged independent wood. */
bool dc_bonfire_admit(dc_gpu_t *gpu,char *err,uint32_t cap);
/** Cold staging for explicitly separate rigid fuel; only ground is terrain.
 * Publish through GPU rigid admission before any visible scene frame. */
bool dc_bonfire_build(const dc_chunk_coord_t *coords,uint32_t count,
    dc_thermal_bench_chunk_t *out,char *err,uint32_t cap);
#endif
