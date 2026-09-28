#include <stdio.h>

#include "gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

static bool make_boundary_pipeline(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDescriptorSetLayoutBinding bindings[18] = {0};
    for (uint32_t i = 0; i < 18u; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1u;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 18u, .pBindings = bindings };
    if (vkCreateDescriptorSetLayout(gpu->device, &layout, NULL,
                                    &gpu->boundary_set_layout) != VK_SUCCESS)
        return error(err, cap, "Cannot create boundary descriptor layout");
    VkDescriptorPoolSize size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 18u };
    VkDescriptorPoolCreateInfo pool = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1u, .poolSizeCount = 1u, .pPoolSizes = &size };
    if (vkCreateDescriptorPool(gpu->device, &pool, NULL,
                               &gpu->boundary_pool) != VK_SUCCESS)
        return error(err, cap, "Cannot create boundary descriptor pool");
    VkDescriptorSetAllocateInfo allocate = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = gpu->boundary_pool, .descriptorSetCount = 1u,
        .pSetLayouts = &gpu->boundary_set_layout };
    if (vkAllocateDescriptorSets(gpu->device, &allocate,
                                 &gpu->boundary_set) != VK_SUCCESS)
        return error(err, cap, "Cannot allocate boundary descriptor set");
    VkPushConstantRange range = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .size = 10u * sizeof(uint32_t) };
    VkPipelineLayoutCreateInfo pipeline_layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1u, .pSetLayouts = &gpu->boundary_set_layout,
        .pushConstantRangeCount = 1u, .pPushConstantRanges = &range };
    if (vkCreatePipelineLayout(gpu->device, &pipeline_layout, NULL,
                               &gpu->boundary_layout) != VK_SUCCESS)
        return error(err, cap, "Cannot create boundary pipeline layout");
    VkShaderModule shader = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/workspace_boundary.comp.spv",
                                   &shader, err, cap)) return false;
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader,
            .pName = "main" }, .layout = gpu->boundary_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE,
        1u, &info, NULL, &gpu->boundary_pipeline);
    vkDestroyShaderModule(gpu->device, shader, NULL);
    if (result != VK_SUCCESS)
        return error(err, cap, "Cannot create boundary compute pipeline");
    return true;
}

bool dc_gpu_boundary_exchange(dc_gpu_t *main_gpu, dc_gpu_t *other_gpu,
                              const dc_gpu_boundary_t *boundaries,
                              uint32_t count, float elapsed_ticks,
                              char *err, uint32_t cap) {
    if (!count) return true;
    if (!main_gpu || !other_gpu || !boundaries ||
        main_gpu->device != other_gpu->device ||
        main_gpu->width < DC_CHUNK_SIDE ||
        main_gpu->height < DC_CHUNK_SIDE ||
        other_gpu->width < DC_CHUNK_SIDE ||
        other_gpu->height < DC_CHUNK_SIDE ||
        !(elapsed_ticks > 0.0f))
        return error(err, cap, "Invalid GPU boundary exchange");
    for (uint32_t i = 0; i < count; ++i)
        if (boundaries[i].main_slot >= main_gpu->slot_capacity ||
            boundaries[i].other_slot >= other_gpu->slot_capacity ||
            boundaries[i].other_side > 3u ||
            boundaries[i].main_x > main_gpu->width - DC_CHUNK_SIDE ||
            boundaries[i].other_x > other_gpu->width - DC_CHUNK_SIDE ||
            boundaries[i].main_y > main_gpu->height - DC_CHUNK_SIDE ||
            boundaries[i].other_y > other_gpu->height - DC_CHUNK_SIDE ||
            boundaries[i].main_x % DC_CHUNK_SIDE ||
            boundaries[i].other_x % DC_CHUNK_SIDE ||
            boundaries[i].main_y % DC_CHUNK_SIDE ||
            boundaries[i].other_y % DC_CHUNK_SIDE)
            return error(err, cap, "Invalid GPU boundary pair");
    if (!main_gpu->boundary_pipeline &&
        !make_boundary_pipeline(main_gpu, err, cap)) return false;
    VkDeviceSize main_cells = (VkDeviceSize)main_gpu->slot_capacity *
                              DC_CHUNK_CELLS * sizeof(dc_cell_t);
    VkDeviceSize other_cells = (VkDeviceSize)other_gpu->slot_capacity *
                               DC_CHUNK_CELLS * sizeof(dc_cell_t);
    VkDeviceSize main_faces = (VkDeviceSize)main_gpu->width * main_gpu->height *
                              sizeof(dc_face_velocity_t);
    VkDeviceSize other_faces = (VkDeviceSize)other_gpu->width * other_gpu->height *
                               sizeof(dc_face_velocity_t);
    VkDeviceSize main_scalars = (VkDeviceSize)main_gpu->width * main_gpu->height *
                                sizeof(uint32_t);
    VkDeviceSize other_scalars = (VkDeviceSize)other_gpu->width * other_gpu->height *
                                 sizeof(uint32_t);
    VkDeviceSize main_particles = (VkDeviceSize)main_gpu->slot_capacity *
        DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t);
    VkDeviceSize other_particles = (VkDeviceSize)other_gpu->slot_capacity *
        DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t);
    VkDeviceSize main_counts = (VkDeviceSize)main_gpu->slot_capacity * sizeof(uint32_t);
    VkDeviceSize other_counts = (VkDeviceSize)other_gpu->slot_capacity * sizeof(uint32_t);
    VkDeviceSize main_markers = (VkDeviceSize)main_gpu->slot_capacity *
        DC_MARKERS_PER_CHUNK * sizeof(dc_marker_t);
    VkDeviceSize other_markers = (VkDeviceSize)other_gpu->slot_capacity *
        DC_MARKERS_PER_CHUNK * sizeof(dc_marker_t);
    VkDescriptorBufferInfo infos[18] = {
        { main_gpu->chunk_buffer, 0, main_cells },
        { other_gpu->chunk_buffer, 0, other_cells },
        { main_gpu->velocity_buffer, 0, main_faces },
        { other_gpu->velocity_buffer, 0, other_faces },
        { main_gpu->pressure_a_buffer, 0, main_scalars },
        { other_gpu->pressure_a_buffer, 0, other_scalars },
        { main_gpu->fluid_b_buffer, 0, main_scalars },
        { other_gpu->fluid_b_buffer, 0, other_scalars },
        { main_gpu->occupancy_buffer, 0, main_scalars },
        { other_gpu->occupancy_buffer, 0, other_scalars },
        { main_gpu->particle_buffer, 0, main_particles },
        { other_gpu->particle_buffer, 0, other_particles },
        { main_gpu->particle_count_buffer, 0, main_counts },
        { other_gpu->particle_count_buffer, 0, other_counts },
        { main_gpu->marker_ping ? main_gpu->marker_b_buffer :
                                  main_gpu->marker_a_buffer, 0, main_markers },
        { other_gpu->marker_ping ? other_gpu->marker_b_buffer :
                                   other_gpu->marker_a_buffer, 0, other_markers },
        { main_gpu->marker_ping ? main_gpu->marker_count_b_buffer :
                                  main_gpu->marker_count_a_buffer, 0, main_counts },
        { other_gpu->marker_ping ? other_gpu->marker_count_b_buffer :
                                   other_gpu->marker_count_a_buffer, 0, other_counts } };
    VkWriteDescriptorSet writes[18] = {0};
    for (uint32_t i = 0; i < 18u; ++i) {
        writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = main_gpu->boundary_set, .dstBinding = i,
            .descriptorCount = 1u,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &infos[i] };
    }
    vkUpdateDescriptorSets(main_gpu->device, 18u, writes, 0u, NULL);
    if (vkResetCommandBuffer(main_gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset boundary command buffer");
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(main_gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin boundary command buffer");
    vkCmdBindPipeline(main_gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      main_gpu->boundary_pipeline);
    vkCmdBindDescriptorSets(main_gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        main_gpu->boundary_layout, 0u, 1u, &main_gpu->boundary_set, 0u, NULL);
    for (uint32_t i = 0; i < count; ++i) {
        const dc_gpu_boundary_t *b = &boundaries[i];
        uint32_t push[10] = { b->main_slot, b->other_slot,
            b->main_x, b->main_y, b->other_x, b->other_y,
            b->other_side, dc_gpu_float_bits(elapsed_ticks),
            main_gpu->width, other_gpu->width };
        vkCmdPushConstants(main_gpu->command, main_gpu->boundary_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(main_gpu->command, 1u, 1u, 1u);
        VkMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                             VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
        VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1u, .pMemoryBarriers = &barrier };
        vkCmdPipelineBarrier2(main_gpu->command, &dependency);
    }
    if (vkEndCommandBuffer(main_gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end boundary command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u, .pCommandBuffers = &main_gpu->command };
    if (vkQueueSubmit(main_gpu->queue, 1u, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(main_gpu->queue) != VK_SUCCESS)
        return error(err, cap, "GPU boundary exchange failed");
    main_gpu->fluid_snapshot_valid = false;
    other_gpu->fluid_snapshot_valid = false;
    return true;
}
