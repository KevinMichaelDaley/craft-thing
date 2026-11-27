#include <string.h>

#include "liquid_seam_gpu_internal.h"
#include "thermal_gpu_internal.h"

typedef struct {
    dc_gpu_seam_push_t seam;
    uint32_t thermal;
} water_publication_push_t;

_Static_assert(sizeof(water_publication_push_t) == 12u * sizeof(uint32_t), "Water publication push mismatch");

bool dc_gpu_prepare_water_publication(dc_gpu_t *gpu, dc_gpu_t *peer,
    fr_renderer_compute_t *compute, const VkDescriptorBufferInfo scratch[DC_WATER_PUBLICATION_SCRATCH_BINDINGS],
    char *err, uint32_t cap) {
    enum { OWNER_BINDINGS = 7u, BINDINGS = DC_WATER_PUBLICATION_SCRATCH_BINDINGS + 2u * OWNER_BINDINGS };
    VkDescriptorBufferInfo buffers[BINDINGS];
    memcpy(buffers, scratch, DC_WATER_PUBLICATION_SCRATCH_BINDINGS * sizeof(*buffers));
    const dc_gpu_t *owners[] = {gpu, peer};
    for (uint32_t i = 0; i < 2u; ++i) {
        const dc_gpu_t *owner = owners[i];
        const VkBuffer heat = fr_thermal_gpu_water_buffer(owner);
        VkDescriptorBufferInfo *out = buffers + DC_WATER_PUBLICATION_SCRATCH_BINDINGS + i * OWNER_BINDINGS;
        out[0] = (VkDescriptorBufferInfo){owner->chunk_buffer, 0, VK_WHOLE_SIZE};
        out[1] = (VkDescriptorBufferInfo){owner->water_color_buffer, 0, VK_WHOLE_SIZE};
        out[2] = (VkDescriptorBufferInfo){owner->velocity_buffer, 0, VK_WHOLE_SIZE};
        out[3] = (VkDescriptorBufferInfo){heat ? heat : owner->thermal_control_buffer, 0, VK_WHOLE_SIZE};
        const VkBuffer markers = owner->marker_ping ? owner->marker_b_buffer : owner->marker_a_buffer;
        const VkBuffer counts = owner->marker_ping ? owner->marker_count_b_buffer : owner->marker_count_a_buffer;
        out[4] = (VkDescriptorBufferInfo){markers, 0, VK_WHOLE_SIZE};
        out[5] = (VkDescriptorBufferInfo){counts, 0, VK_WHOLE_SIZE};
        out[6] = (VkDescriptorBufferInfo){owner->marker_grid_buffer, 0, VK_WHOLE_SIZE};
    }
    if (compute->pipeline) {
        dc_gpu_seam_bind_buffers(gpu, compute->descriptor, buffers, BINDINGS);
        return true;
    }
    if (fr_renderer_compute_init(gpu, compute, buffers, BINDINGS, sizeof(water_publication_push_t),
                                  "build/shaders/liquid_water_publication.comp.spv", err, cap))
        return true;
    fr_renderer_compute_destroy(gpu, compute);
    *compute = (fr_renderer_compute_t){0};
    return false;
}

void dc_gpu_record_water_publication(dc_gpu_t *gpu, const fr_renderer_compute_t *compute,
    dc_gpu_seam_push_t seam, bool thermal) {
    const water_publication_push_t push = {seam, thermal};
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        compute->pipeline_layout, 0, 1, &compute->descriptor, 0, NULL);
    vkCmdPushConstants(gpu->command, compute->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
    vkCmdDispatch(gpu->command, 1, 1, 1);
}
