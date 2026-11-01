#ifndef FERRUM_PLAYER_DRIVE_GPU_H
#define FERRUM_PLAYER_DRIVE_GPU_H

#include "dungeoncraft/gpu.h"

typedef struct fr_player_drive_gpu fr_player_drive_gpu_t;
typedef struct {uint64_t identity;uint32_t body_id,generation;} fr_player_body_binding_t;
typedef struct {
    uint64_t identity,sequence;
    float move_x;
    bool jump_pressed;
} fr_player_drive_input_t;

/** Cold creation with unique player/body bindings and explicit motion tuning.
 * Speeds are cells per fixed physics tick, finite in [0,4]. This API defines no
 * attributes or RPG rules. Keep bindings valid until destruction; generation
 * mismatch, missing bodies and kinematic bodies are no-ops on the GPU.
 * Caller owns the returned adapter; the GPU must outlive it. */
bool fr_player_drive_gpu_create(dc_gpu_t *gpu,const fr_player_body_binding_t *bindings,
    uint32_t count,float speed_per_tick,float jump_speed_per_tick,
    fr_player_drive_gpu_t **out,char *err_buf,uint32_t err_cap);
/** Apply one input per bound player at a completed fixed-step boundary, before
 * the real rigid solve. Uses GPU body records and the solver's support flag;
 * never reads bodies to CPU or moves transforms. Strictly increasing nonzero
 * sequence numbers suppress retries. Input must follow binding order; invalid
 * input rejects the entire submission. Jump is a press, not a held action.
 * Separate adapters must never bind the same body concurrently. */
bool fr_player_drive_gpu_submit(fr_player_drive_gpu_t *drive,
    const fr_player_drive_input_t *inputs,uint32_t count,char *err_buf,uint32_t err_cap);
/** Release adapter storage after submitted commands complete. */
void fr_player_drive_gpu_destroy(fr_player_drive_gpu_t *drive);

#endif
