#ifndef FERRUM_PLAYER_FIRE_RAY_GPU_H
#define FERRUM_PLAYER_FIRE_RAY_GPU_H

#include "ferrum/player/drive_gpu.h"

typedef struct fr_fire_ray_gpu fr_fire_ray_gpu_t;
typedef struct {uint32_t range,half_height;float duration,temperature_K;} fr_fire_ray_config_t;
typedef struct {uint64_t identity,sequence;int32_t facing;bool emit;} fr_fire_ray_input_t;
typedef struct {uint32_t casts,heated,blocked_rows;float remaining;} fr_fire_ray_stats_t;
/** Create explicit test-spell tuning and exclusive generation-bearing emitters.
 * No implicit casts. The GPU must outlive this adapter. Ray heat is delivered
 * to existing parcel/atmosphere energy, never to a cosmetic reaction solver. */
bool fr_fire_ray_gpu_create(dc_gpu_t *gpu,const fr_player_body_binding_t *bindings,
    uint32_t count,fr_fire_ray_config_t config,fr_fire_ray_gpu_t **out,char *err,uint32_t cap);
/** Advance existing rays; only emit=true with a newer nonzero sequence starts
 * one. Direction is latched at emission. Air and fuel-bearing substrates pass;
 * nonfuel solids, water and missing pages stop each traced row. */
bool fr_fire_ray_gpu_submit(fr_fire_ray_gpu_t *ray,const fr_fire_ray_input_t *inputs,
    uint32_t count,float seconds,char *err,uint32_t cap);
/** Completed scalar diagnostic; never used by gameplay scheduling. */
bool fr_fire_ray_gpu_stats(fr_fire_ray_gpu_t *ray,uint32_t index,fr_fire_ray_stats_t *out);
/** Detach presentation and release resources at a completed command boundary. */
void fr_fire_ray_gpu_destroy(fr_fire_ray_gpu_t *ray);

#endif
