#ifndef FERRUM_PHYSICS_AUTHORED_RIGID_GPU_H
#define FERRUM_PHYSICS_AUTHORED_RIGID_GPU_H
#include "dungeoncraft/gpu.h"

/** Cold explicit rigid-object admission. The immutable object channel is indexed
 * by physical chunk slot then local texel; zero leaves that texel unselected.
 * Distinct nonzero object IDs cannot merge, even when their material pixels
 * touch. The GPU derives connectivity and allocates real variable-size bodies.
 * Source transaction/object identities are checkpointed with each body's owner.
 */
bool fr_gpu_admit_rigid_objects(dc_gpu_t *gpu,uint64_t transaction,
    const uint32_t *objects,uint32_t object_cells,char *err_buf,uint32_t err_cap);
#endif
