#include "ferrum/physics/authored_rigid_gpu.h"
bool fr_gpu_admit_rigid_objects(dc_gpu_t *gpu,uint64_t transaction,
    const uint32_t *objects,uint32_t object_cells,char *err_buf,uint32_t err_cap) {
    (void)gpu;(void)transaction;(void)objects;(void)object_cells;(void)err_buf;(void)err_cap;
    return false;
}
