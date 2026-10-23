#ifndef DC_RUNTIME_BODY_INTERNAL_H
#define DC_RUNTIME_BODY_INTERNAL_H
#include "body_storage_gpu.h"
#include "renderer_internal.h"
#include "runtime_body_gpu.h"
struct dc_runtime_bodies {
    dc_gpu_address_buffer_t metadata,root,initial_auxiliary,work,mailbox;
    dc_gpu_body_store_t store;
    fr_renderer_compute_t initialize,index,scan;
    uint64_t submission;
    uint32_t force_groups,fragment_cells;
    bool enabled;
};
/** Grow geometry and appearance together, copying payloads only on the GPU. */
bool dc_gpu_runtime_geometry_reserve(dc_gpu_t *gpu,uint32_t words,char *err,uint32_t cap);
/** Grow pair, contact and contact-solver storage from GPU allocation requests. */
bool dc_gpu_runtime_queues_reserve(dc_gpu_t *gpu,uint32_t pairs,uint32_t contacts,char *err,uint32_t cap);
#endif
