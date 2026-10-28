#include "ferrum/physics/revolute_gpu.h"

bool fr_gpu_revolute_add(dc_gpu_t *gpu, const fr_revolute_joint_t *joint,
                         char *err_buf, uint32_t err_cap) {
    (void)gpu; (void)joint; (void)err_buf; (void)err_cap;
    return false;
}
bool fr_gpu_revolute_remove(dc_gpu_t *gpu, uint32_t id,
                            char *err_buf, uint32_t err_cap) {
    (void)gpu; (void)id; (void)err_buf; (void)err_cap;
    return false;
}
