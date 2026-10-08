#include "gpu_internal.h"

bool dc_gpu_set_rigid_solver(dc_gpu_t *gpu, bool enabled) {
    (void)gpu; (void)enabled; return false;
}
bool dc_gpu_set_body_motion(dc_gpu_t *gpu, uint32_t id, float angle, float omega,
                            uint32_t flags, char *err, uint32_t cap) {
    (void)gpu; (void)id; (void)angle; (void)omega; (void)flags; (void)err; (void)cap;
    return false;
}
bool dc_gpu_read_body_motion(dc_gpu_t *gpu, uint32_t id, dc_gpu_body_motion_t *motion,
                             char *err, uint32_t cap) {
    (void)gpu; (void)id; (void)motion; (void)err; (void)cap; return false;
}
bool dc_gpu_read_rigid_solver_stats(dc_gpu_t *gpu, dc_gpu_rigid_solver_stats_t *stats) {
    (void)gpu; (void)stats; return false;
}
