#ifndef FERRUM_PLAYER_BODY_GPU_H
#define FERRUM_PLAYER_BODY_GPU_H

#include "ferrum/player/drive_gpu.h"

typedef struct {
    fr_player_body_binding_t binding;
    uint32_t rest_origin_x,rest_origin_y,width,height;
} fr_beaver_body_t;
/** Cold admission into a fresh scene through one empty resident staging chunk.
 * Admit every canonical owner once, including temporarily occluded source
 * pixels, then place the assembly at the initial spawn interface. The canonical
 * rest geometry currently supplies collision; pose-dependent exposure requires
 * the later deformation adapter. Not a transactional live-spawn API. The caller
 * must abandon a fresh scene if resource failure interrupts admission. */
bool fr_beaver_body_admit(dc_gpu_t *gpu,uint64_t identity,uint64_t transaction,
    uint32_t staging_slot,dc_chunk_coord_t spawn,int32_t x_fp,int32_t y_fp,
    fr_beaver_body_t *out,char *err,uint32_t cap);

#endif
