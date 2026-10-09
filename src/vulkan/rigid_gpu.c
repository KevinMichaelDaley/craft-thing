#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

_Static_assert(sizeof(dc_gpu_body_t) == 32, "GPU body layout must match SPIR-V");
_Static_assert(sizeof(dc_gpu_body_record_t) == 232, "GPU body record must match SPIR-V");
_Static_assert(offsetof(dc_gpu_body_record_t, shape) == 80, "GPU shape offset must match SPIR-V");
_Static_assert(offsetof(dc_gpu_body_record_t, angle) == 152, "GPU angular state offset changed");
_Static_assert(offsetof(dc_gpu_body_record_t, center) == 168, "GPU centroid offset changed");
_Static_assert(offsetof(dc_gpu_body_record_t, bounds_low) == 200, "GPU cached bounds offset changed");

enum { RIGID_RASTER = 1u, RIGID_PREPARE = 2u, RIGID_PHYSICAL_MASK = 128u };

static bool submit_rigid(dc_gpu_t *gpu, bool advance, char *err, uint32_t cap);
static void record_rigid(dc_gpu_t *gpu, bool advance);

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

static uint32_t body_slot(const dc_gpu_t *gpu, uint32_t id) {
    for (uint32_t i = 0; i < gpu->body_count; ++i)
        if (gpu->body_ids[i] == id) return i;
    return DC_GPU_BODY_CAPACITY;
}

bool dc_gpu_spawn_compound_body(dc_gpu_t *gpu, dc_gpu_world_body_t body,
                                const dc_gpu_compound_shape_t *shape, char *err, uint32_t cap) {
    (void)gpu; (void)body; (void)shape;
    return error(err, cap, "Compound bodies not implemented");
}
bool dc_gpu_read_compound_shape(dc_gpu_t *gpu, uint32_t id, dc_gpu_compound_shape_t *shape,
                                char *err, uint32_t cap) {
    (void)gpu; (void)id; (void)shape;
    return error(err, cap, "Compound bodies not implemented");
}

bool dc_gpu_valid_body_shape(const dc_gpu_body_t *body, const dc_gpu_body_shape_t *shape) {
    if (!body || !shape || shape->count < 3 || shape->count > DC_GPU_CONVEX_VERTICES ||
        (shape->material != DC_GPU_BODY_STONE && shape->material != DC_GPU_BODY_WOOD) ||
        !body->width || !body->height || body->width > DC_GPU_BODY_MAX_SIDE ||
        body->height > DC_GPU_BODY_MAX_SIDE) return false;
    for (uint32_t i = 0; i < shape->count; ++i) {
        int64_t x = shape->vertices[i].x_fp, y = shape->vertices[i].y_fp;
        if (x < 0 || y < 0 || x > (int64_t)body->width * DC_FLUID_FULL ||
            y > (int64_t)body->height * DC_FLUID_FULL) return false;
    }
    for (uint32_t i = 0; i < shape->count; ++i) {
        uint32_t next = (i + 1) % shape->count;
        int64_t x = shape->vertices[i].x_fp, y = shape->vertices[i].y_fp;
        int64_t dx = shape->vertices[next].x_fp - x, dy = shape->vertices[next].y_fp - y;
        for (uint32_t j = 0; j < shape->count; ++j) {
            if (j == i || j == next) continue;
            int64_t px = shape->vertices[j].x_fp - x, py = shape->vertices[j].y_fp - y;
            if (dx * py - dy * px <= 0) return false;
        }
    }
    return true;
}

bool dc_gpu_spawn_convex_body(dc_gpu_t *gpu, dc_gpu_world_body_t body,
                              const dc_gpu_body_shape_t *shape, char *err, uint32_t cap) {
    if (!dc_gpu_valid_body_shape(&body.body, shape))
        return error(err, cap, "Invalid convex body shape");
    dc_gpu_body_shape_t copy = { .count = shape->count, .material = shape->material };
    memcpy(copy.vertices, shape->vertices, shape->count * sizeof(shape->vertices[0]));
    if (!dc_gpu_spawn_world_body(gpu, body, err, cap)) return false;
    ((dc_gpu_body_record_t *)gpu->body_mapped)[body_slot(gpu, body.body.id)].shape = copy;
    return true;
}

bool dc_gpu_read_body_shape(dc_gpu_t *gpu, uint32_t id, dc_gpu_body_shape_t *shape,
                             char *err, uint32_t cap) {
    if (!gpu || !shape || !id) return error(err, cap, "Invalid body shape readback");
    uint32_t slot = body_slot(gpu, id);
    if (slot == DC_GPU_BODY_CAPACITY) return error(err, cap, "GPU body ID not found");
    *shape = ((dc_gpu_body_record_t *)gpu->body_mapped)[slot].shape;
    return true;
}

uint32_t dc_gpu_next_body_id(const dc_gpu_t *gpu) {
    if (!gpu) return 0;
    for (uint32_t id = 1; id <= DC_GPU_BODY_CAPACITY + 1; ++id)
        if (body_slot(gpu, id) == DC_GPU_BODY_CAPACITY) return id;
    return 0;
}

bool dc_gpu_rigid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    bool okay = dc_gpu_make_mapped_buffer(gpu, dc_gpu_body_storage_bytes(gpu),
               &gpu->body_buffer, &gpu->body_memory, &gpu->body_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu,
               (VkDeviceSize)gpu->width * gpu->height * sizeof(uint32_t) * 2,
               &gpu->occupancy_buffer, &gpu->occupancy_memory,
               &gpu->occupancy_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, 3 * sizeof(uint32_t),
               &gpu->trace_buffer, &gpu->trace_memory,
               &gpu->trace_mapped, err, cap);
    if (okay) {
        dc_gpu_broadphase_data(gpu)[DC_GPU_BROADPHASE_CAPACITY_WORD] = DC_GPU_BROADPHASE_PAIR_CAPACITY;
        dc_gpu_broadphase_data(gpu)[DC_GPU_BROADPHASE_BUCKETS_WORD] = dc_gpu_broadphase_buckets(gpu);
        dc_gpu_contact_data(gpu)[DC_GPU_CONTACT_CAPACITY_WORD] = DC_GPU_CONTACT_CAPACITY;
    }
    return okay;
}

bool dc_gpu_rigid_pipeline_init(dc_gpu_t *gpu, const char *path,
                                char *err, uint32_t cap) {
    VkShaderModule shader = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, path, &shader, err, cap)) return false;
    VkComputePipelineCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1,
        &info, NULL, &gpu->rigid_pipeline);
    vkDestroyShaderModule(gpu->device, shader, NULL);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create rigid pipeline");
    return true;
}

void dc_gpu_rigid_destroy(dc_gpu_t *gpu) {
    dc_gpu_tick_destroy(gpu);
    if (gpu->contact_pipeline) vkDestroyPipeline(gpu->device, gpu->contact_pipeline, NULL);
    for (uint32_t i=0;i<DC_GPU_SOLVER_PHASE_COUNT;++i)
        if (gpu->solver_pipelines[i]) vkDestroyPipeline(gpu->device,gpu->solver_pipelines[i],NULL);
    if (gpu->broadphase_pipeline) vkDestroyPipeline(gpu->device, gpu->broadphase_pipeline, NULL);
    if (gpu->rigid_pipeline) vkDestroyPipeline(gpu->device, gpu->rigid_pipeline, NULL);
    if (gpu->body_mapped) vkUnmapMemory(gpu->device, gpu->body_memory);
    if (gpu->occupancy_mapped) vkUnmapMemory(gpu->device, gpu->occupancy_memory);
    if (gpu->body_buffer) vkDestroyBuffer(gpu->device, gpu->body_buffer, NULL);
    if (gpu->occupancy_buffer) vkDestroyBuffer(gpu->device, gpu->occupancy_buffer, NULL);
    if (gpu->body_memory) vkFreeMemory(gpu->device, gpu->body_memory, NULL);
    if (gpu->occupancy_memory) vkFreeMemory(gpu->device, gpu->occupancy_memory, NULL);
}

bool dc_gpu_spawn_body(dc_gpu_t *gpu, dc_gpu_body_t body,
                       char *err, uint32_t cap) {
    if (!gpu || !body.active || !body.id || !body.width || !body.height ||
        body.width > DC_GPU_BODY_MAX_SIDE || body.height > DC_GPU_BODY_MAX_SIDE || body.width > gpu->width ||
        body.height > gpu->height || body.x_fp < 0 || body.y_fp < 0 ||
        (uint32_t)(body.x_fp >> 16) > gpu->width - body.width ||
        (uint32_t)(body.y_fp >> 16) > gpu->height - body.height)
        return error(err, cap, "Invalid GPU rigid body");
    int32_t chunk_x = body.x_fp / (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL);
    int32_t chunk_y = body.y_fp / (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL);
    if (gpu->body_origin.x > INT64_MAX - chunk_x ||
        gpu->body_origin.y > INT64_MAX - chunk_y)
        return error(err, cap, "World body anchor overflow");
    dc_gpu_world_body_t world = { .chunk = {
        gpu->body_origin.x + chunk_x, gpu->body_origin.y + chunk_y }, .body = body };
    world.body.x_fp %= (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL);
    world.body.y_fp %= (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL);
    if (!dc_gpu_spawn_world_body(gpu, world, err, cap)) return false;
    ((dc_gpu_body_record_t *)gpu->body_mapped)[body_slot(gpu, body.id)].body = body;
    return true;
}

bool dc_gpu_read_body(dc_gpu_t *gpu, dc_gpu_body_t *body,
                      char *err, uint32_t cap) {
    if (!gpu || !body) return error(err, cap, "Invalid GPU rigid readback");
    memset(body, 0, sizeof(*body));
    for (uint32_t i = 0; i < gpu->body_count; ++i) {
        if (gpu->body_ids[i]) {
            *body = ((dc_gpu_body_record_t *)gpu->body_mapped)[i].body;
            break;
        }
    }
    return true;
}

bool dc_gpu_read_body_id(dc_gpu_t *gpu, uint32_t id, dc_gpu_body_t *body,
                         char *err, uint32_t cap) {
    if (!gpu || !body || !id)
        return error(err, cap, "Invalid GPU body ID readback");
    uint32_t slot = body_slot(gpu, id);
    if (slot == DC_GPU_BODY_CAPACITY) return error(err, cap, "GPU body ID not found");
    *body = ((dc_gpu_body_record_t *)gpu->body_mapped)[slot].body;
    return true;
}

bool dc_gpu_remove_body(dc_gpu_t *gpu, uint32_t id, char *err, uint32_t cap) {
    if (!gpu || !id) return error(err, cap, "Invalid GPU body removal");
    uint32_t slot = body_slot(gpu, id);
    if (slot == DC_GPU_BODY_CAPACITY) return error(err, cap, "GPU body ID not found");
    memset(&((dc_gpu_body_record_t *)gpu->body_mapped)[slot], 0,
           sizeof(dc_gpu_body_record_t));
    gpu->body_ids[slot] = 0;
    gpu->body_refresh_pending = true;
    while (gpu->body_count && !gpu->body_ids[gpu->body_count - 1]) --gpu->body_count;
    return true;
}

bool dc_gpu_read_occupancy(dc_gpu_t *gpu, uint32_t x, uint32_t y,
                           uint32_t *current, uint32_t *swept,
                           char *err, uint32_t cap) {
    if (!gpu || x >= gpu->width || y >= gpu->height || (!current && !swept))
        return error(err, cap, "Invalid GPU occupancy readback");
    const uint32_t *cells = gpu->occupancy_mapped;
    uint32_t value = cells[(size_t)y * gpu->width + x];
    if (current) *current = value;
    if (swept) *swept = cells[(size_t)gpu->width * gpu->height + (size_t)y * gpu->width + x];
    return true;
}

bool dc_gpu_set_body_origin(dc_gpu_t *gpu, dc_chunk_coord_t origin,
                             char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "Invalid body origin");
    bool changed = origin.x != gpu->body_origin.x || origin.y != gpu->body_origin.y;
    gpu->body_origin = origin;
    if ((!changed && !gpu->body_refresh_pending) ||
        (!gpu->body_count && !gpu->rigid_occupancy_present)) return true;
    return submit_rigid(gpu, false, err, cap);
}

bool dc_gpu_spawn_world_body(dc_gpu_t *gpu, dc_gpu_world_body_t body,
                              char *err, uint32_t cap) {
    if (!gpu || !body.body.id || !body.body.active || !body.body.width ||
        !body.body.height || body.body.width > DC_GPU_BODY_MAX_SIDE ||
        body.body.height > DC_GPU_BODY_MAX_SIDE ||
        body.body.x_fp < 0 || body.body.y_fp < 0 ||
        body.body.x_fp >= (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL) ||
        body.body.y_fp >= (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL))
        return error(err, cap, "Invalid world body");
    uint32_t slot = body_slot(gpu, body.body.id);
    if (slot == DC_GPU_BODY_CAPACITY)
        for (uint32_t i = 0; i < DC_GPU_BODY_CAPACITY; ++i)
            if (!gpu->body_ids[i]) { slot = i; break; }
    if (slot == DC_GPU_BODY_CAPACITY) return error(err, cap, "GPU body pool is full");
    uint64_t x = (uint64_t)body.chunk.x, y = (uint64_t)body.chunk.y;
    dc_gpu_body_record_t *records = gpu->body_mapped;
    records[slot] = (dc_gpu_body_record_t){ .body = body.body,
        .chunk_x = {(uint32_t)x, (uint32_t)(x >> 32)},
        .chunk_y = {(uint32_t)y, (uint32_t)(y >> 32)},
        .local_x_fp = body.body.x_fp, .local_y_fp = body.body.y_fp };
    gpu->body_ids[slot] = body.body.id;
    if (gpu->body_count <= slot) gpu->body_count = slot + 1;
    gpu->body_refresh_pending = true;
    return true;
}

bool dc_gpu_read_world_body(dc_gpu_t *gpu, uint32_t id, dc_gpu_world_body_t *body,
                             char *err, uint32_t cap) {
    if (!gpu || !body || !id) return error(err, cap, "Invalid world body readback");
    uint32_t slot = body_slot(gpu, id);
    if (slot == DC_GPU_BODY_CAPACITY) return error(err, cap, "GPU body ID not found");
    const dc_gpu_body_record_t *record = &((dc_gpu_body_record_t *)gpu->body_mapped)[slot];
    uint64_t x = record->chunk_x[0] | ((uint64_t)record->chunk_x[1] << 32);
    uint64_t y = record->chunk_y[0] | ((uint64_t)record->chunk_y[1] << 32);
    *body = (dc_gpu_world_body_t){ .body = record->body };
    memcpy(&body->chunk.x, &x, sizeof(x));
    memcpy(&body->chunk.y, &y, sizeof(y));
    body->body.x_fp = record->local_x_fp;
    body->body.y_fp = record->local_y_fp;
    return true;
}

bool dc_gpu_rigid_step(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return submit_rigid(gpu, true, err, cap);
}

static bool submit_rigid(dc_gpu_t *gpu, bool advance, char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset rigid command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin rigid command buffer");
    record_rigid(gpu, advance);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end rigid command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "GPU rigid submission failed");
    return true;
}

void dc_gpu_record_rigid(dc_gpu_t *gpu) {
    record_rigid(gpu, true);
}

static void record_rigid(dc_gpu_t *gpu, bool advance) {
    if (!gpu->body_count && !gpu->rigid_occupancy_present) return;
    VkMemoryBarrier2 upload = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
    VkDependencyInfo dep = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &upload };
    vkCmdPipelineBarrier2(gpu->command, &dep);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->rigid_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint64_t x = (uint64_t)gpu->body_origin.x, y = (uint64_t)gpu->body_origin.y;
    uint32_t push[8] = { gpu->width, gpu->height, advance && !gpu->rigid_solver_enabled ? 0u : RIGID_PREPARE, gpu->body_count,
        (uint32_t)x, (uint32_t)(x >> 32), (uint32_t)y, (uint32_t)(y >> 32) };
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    if (gpu->body_count) vkCmdDispatch(gpu->command, 1, 1, 1);
    VkMemoryBarrier2 between = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
    dep.pMemoryBarriers = &between;
    vkCmdPipelineBarrier2(gpu->command, &dep);
    if (advance && gpu->rigid_solver_enabled && gpu->body_count) {
        dc_gpu_record_solver(gpu);
        vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->rigid_pipeline);
    }
    push[2] = RIGID_RASTER | (gpu->rigid_solver_enabled ? RIGID_PHYSICAL_MASK : 0u);
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, (gpu->width + 15u) / 16u,
        (gpu->height + 15u) / 16u, 1);
    VkMemoryBarrier2 finish = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT };
    dep.pMemoryBarriers = &finish;
    vkCmdPipelineBarrier2(gpu->command, &dep);
    dc_gpu_record_broadphase(gpu);
    dc_gpu_record_contacts(gpu);
    gpu->rigid_occupancy_present = gpu->body_count != 0;
    gpu->body_refresh_pending = false;
}
