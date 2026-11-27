#include "liquid_seam_gpu_internal.h"
#include "thermal_gpu_internal.h"
#include "thermal_gpu_state.h"

typedef struct {
    uint32_t first_base, second_base, first_stride, second_stride;
    uint32_t first_cells, second_cells, axis, first_before, gameplay;
    float seconds;
} thermal_face_push_t;

bool fr_thermal_prepare_liquid_seam(dc_gpu_t *first, dc_gpu_t *second, char *err, uint32_t cap) {
    if (!first->thermal) return true;
    fr_renderer_compute_t *compute = &first->thermal->liquid_seam;
    VkDescriptorBufferInfo buffers[] = {
        {first->thermal->samples.buffer, 0, VK_WHOLE_SIZE}, {second->thermal->samples.buffer, 0, VK_WHOLE_SIZE},
        {first->thermal->fluxes.buffer, 0, VK_WHOLE_SIZE}, {second->thermal->fluxes.buffer, 0, VK_WHOLE_SIZE}
    };
    if (compute->pipeline) {
        dc_gpu_seam_bind_buffers(first, compute->descriptor, buffers, 4u);
        return true;
    }
    if (fr_renderer_compute_init(first, compute, buffers, 4u, sizeof(thermal_face_push_t),
        "build/shaders/thermal_liquid_seam.comp.spv", err, cap)) return true;
    fr_renderer_compute_destroy(first, compute);
    *compute = (fr_renderer_compute_t){0};
    return false;
}

static uint32_t face_base(dc_liquid_seam_endpoint_t tile, bool vertical, bool high) {
    uint32_t x = tile.tile_x * DC_CHUNK_SIDE, y = tile.tile_y * DC_CHUNK_SIDE;
    if (high && vertical) y += DC_CHUNK_SIDE - 1u;
    if (high && !vertical) x += DC_CHUNK_SIDE - 1u;
    return y * tile.gpu->width + x;
}

void fr_thermal_record_liquid_seams(dc_gpu_t *first, const dc_liquid_seam_endpoint_t *tiles,
    uint32_t count, float seconds) {
    const fr_renderer_compute_t *compute = &first->thermal->liquid_seam;
    vkCmdBindPipeline(first->command, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline);
    vkCmdBindDescriptorSets(first->command, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline_layout,
        0, 1, &compute->descriptor, 0, NULL);
    for (uint32_t i = 0; i < count; ++i) {
        if (tiles[i].gpu != first) continue;
        for (uint32_t j = 0; j < count; ++j) {
            dc_liquid_seam_endpoint_t a = tiles[i], b = tiles[j];
            if (b.gpu == first) continue;
            bool vertical = a.coord.x == b.coord.x;
            if (!vertical && a.coord.y != b.coord.y) continue;
            int64_t u = vertical ? a.coord.y : a.coord.x, v = vertical ? b.coord.y : b.coord.x;
            bool before = u < v;
            if ((before ? (uint64_t)v - (uint64_t)u : (uint64_t)u - (uint64_t)v) != 1u) continue;
            thermal_face_push_t push = {face_base(a, vertical, before), face_base(b, vertical, !before),
                vertical ? 1u : first->width, vertical ? 1u : b.gpu->width,
                first->width * first->height, b.gpu->width * b.gpu->height, vertical, before,
                first->thermal->purpose == FR_THERMAL_WORLD_BONFIRE, seconds};
            fr_renderer_barrier(first->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            vkCmdPushConstants(first->command, compute->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                0, sizeof(push), &push);
            vkCmdDispatch(first->command, 1, 1, 1);
        }
    }
}
