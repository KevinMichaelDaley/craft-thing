#ifndef FERRUM_PHYSICS_REVOLUTE_GPU_H
#define FERRUM_PHYSICS_REVOLUTE_GPU_H
#include "dungeoncraft/gpu.h"

/** Local anchors are relative to each body's rest-space center of mass. */
typedef struct {
    uint32_t id, body_a, body_b;
    float anchor_a[2], anchor_b[2];
} fr_revolute_joint_t;

/** Admit a generation-bound two-body pivot at an idle authoring boundary. */
bool fr_gpu_revolute_add(dc_gpu_t *gpu, const fr_revolute_joint_t *joint,
                         char *err_buf, uint32_t err_cap);
/** Release a pivot without changing either body's pose or velocity. */
bool fr_gpu_revolute_remove(dc_gpu_t *gpu, uint32_t id,
                            char *err_buf, uint32_t err_cap);
#endif
