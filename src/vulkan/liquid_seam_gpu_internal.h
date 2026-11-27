#ifndef DUNGEONCRAFT_LIQUID_SEAM_GPU_INTERNAL_H
#define DUNGEONCRAFT_LIQUID_SEAM_GPU_INTERNAL_H

#include "dungeoncraft/liquid_seam.h"
#include "renderer_internal.h"

typedef struct {
    uint32_t slot, x, y, width, height;
} dc_gpu_seam_location_t;

typedef struct {
    dc_gpu_seam_location_t first, second;
    uint32_t axis;
} dc_gpu_seam_push_t;

enum { DC_WATER_PUBLICATION_SCRATCH_BINDINGS = 9u };

dc_gpu_seam_push_t dc_gpu_seam_push(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis);
void dc_gpu_seam_bind_buffers(dc_gpu_t *gpu, VkDescriptorSet set,
    const VkDescriptorBufferInfo *buffers, uint32_t count);
bool dc_gpu_prepare_liquid_receipt(dc_gpu_t *gpu, char *err, uint32_t cap);
dc_liquid_seam_result_t dc_gpu_submit_liquid_seam(dc_gpu_t *gpu, dc_gpu_t *peer,
    const uint32_t *receipt, bool bulk, char *err, uint32_t cap);

bool dc_gpu_prepare_water_publication(dc_gpu_t *gpu, dc_gpu_t *peer,
    fr_renderer_compute_t *compute, const VkDescriptorBufferInfo scratch[DC_WATER_PUBLICATION_SCRATCH_BINDINGS],
    char *err, uint32_t cap);
void dc_gpu_record_water_publication(dc_gpu_t *gpu, const fr_renderer_compute_t *compute,
    dc_gpu_seam_push_t seam, bool thermal);

#endif
