#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/assets/prop.h"
#include "ferrum/structural/authoring.h"
#include "gpu_internal.h"

enum { STAGE_HEADER_WORDS = 24, STAGE_WORDS_PER_PIXEL = 20,
       STAGE_PLAN_PREFIX_WORDS = 12, STAGE_PLAN_WORDS = 22, STAGE_PHASES = 8 };
_Static_assert(sizeof(fr_authored_component_plan_t) == 10u * sizeof(uint32_t),
               "Authored component plan GPU layout changed");
struct dc_gpu_authored_stage {
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint32_t *mapped;
    VkDescriptorSetLayout set_layout;
    VkDescriptorPool pool;
    VkDescriptorSet descriptor;
    VkPipelineLayout layout;
    VkPipeline pipelines[STAGE_PHASES];
    uint32_t pixels;
    uint64_t local_bytes, system_bytes;
    bool ready;
};

static bool error(char *err, uint32_t cap, const char *reason) {
    if (err && cap) snprintf(err, cap, "%s", reason);
    return false;
}

void dc_gpu_authored_destroy(dc_gpu_t *gpu) {
    struct dc_gpu_authored_stage *s = gpu->authored_stage;
    if (!s) return;
    for (unsigned i = 0; i < STAGE_PHASES; i++)
        if (s->pipelines[i]) vkDestroyPipeline(gpu->device, s->pipelines[i], NULL);
    if (s->layout) vkDestroyPipelineLayout(gpu->device, s->layout, NULL);
    if (s->pool) vkDestroyDescriptorPool(gpu->device, s->pool, NULL);
    if (s->set_layout) vkDestroyDescriptorSetLayout(gpu->device, s->set_layout, NULL);
    if (s->mapped) vkUnmapMemory(gpu->device, s->memory);
    if (s->buffer) vkDestroyBuffer(gpu->device, s->buffer, NULL);
    if (s->memory) vkFreeMemory(gpu->device, s->memory, NULL);
    gpu->mapped_local_bytes -= s->local_bytes;
    gpu->mapped_system_bytes -= s->system_bytes;
    free(s);
    gpu->authored_stage = NULL;
}

static bool stage_init(dc_gpu_t *gpu, uint32_t count, char *err, uint32_t cap) {
    VkDeviceSize bytes = ((VkDeviceSize)STAGE_HEADER_WORDS +
                          (VkDeviceSize)count * (STAGE_WORDS_PER_PIXEL + STAGE_PLAN_WORDS) +
                          DC_STRUCTURAL_GRAPH_WORDS) * sizeof(uint32_t);
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(gpu->physical, &properties);
    if (bytes > properties.limits.maxStorageBufferRange)
        return error(err, cap, "Authored staging exceeds device buffer range");
    struct dc_gpu_authored_stage *s = calloc(1, sizeof(*s));
    if (!s) return error(err, cap, "Cannot allocate authored staging context");
    gpu->authored_stage = s;
    s->pixels = count;
    void *mapped = NULL;
    uint64_t local_before = gpu->mapped_local_bytes;
    uint64_t system_before = gpu->mapped_system_bytes;
    if (!dc_gpu_make_mapped_buffer(gpu, bytes, &s->buffer, &s->memory, &mapped, err, cap)) return false;
    s->local_bytes = gpu->mapped_local_bytes - local_before;
    s->system_bytes = gpu->mapped_system_bytes - system_before;
    s->mapped = mapped;
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
    VkDescriptorSetLayoutCreateInfo set = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1, .pBindings = &binding};
    if (vkCreateDescriptorSetLayout(gpu->device, &set, NULL, &s->set_layout) != VK_SUCCESS)
        return error(err, cap, "Cannot create authored descriptor layout");
    VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    VkDescriptorPoolCreateInfo pool = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &size};
    if (vkCreateDescriptorPool(gpu->device, &pool, NULL, &s->pool) != VK_SUCCESS)
        return error(err, cap, "Cannot create authored descriptor pool");
    VkDescriptorSetAllocateInfo allocation = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = s->pool, .descriptorSetCount = 1, .pSetLayouts = &s->set_layout};
    if (vkAllocateDescriptorSets(gpu->device, &allocation, &s->descriptor) != VK_SUCCESS)
        return error(err, cap, "Cannot allocate authored descriptor");
    VkDescriptorBufferInfo buffer = {s->buffer, 0, bytes};
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = s->descriptor, .dstBinding = 0, .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffer};
    vkUpdateDescriptorSets(gpu->device, 1, &write, 0, NULL);
    gpu->descriptor_revision++;
    VkPushConstantRange push = {VK_SHADER_STAGE_COMPUTE_BIT, 0, 16};
    VkPipelineLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &s->set_layout,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push};
    if (vkCreatePipelineLayout(gpu->device, &layout, NULL, &s->layout) != VK_SUCCESS)
        return error(err, cap, "Cannot create authored pipeline layout");
    VkShaderModule module;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/authored.comp.spv", &module, err, cap)) return false;
    VkSpecializationMapEntry entry = {0, 0, sizeof(uint32_t)};
    VkResult result = VK_SUCCESS;
    for (uint32_t phase = 0; phase < STAGE_PHASES && result == VK_SUCCESS; phase++) {
        VkSpecializationInfo specialization = {
            .mapEntryCount = 1, .pMapEntries = &entry,
            .dataSize = sizeof(phase), .pData = &phase};
        VkComputePipelineCreateInfo pipeline = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module,
                      .pName = "main", .pSpecializationInfo = &specialization},
            .layout = s->layout};
        result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1, &pipeline, NULL, &s->pipelines[phase]);
    }
    vkDestroyShaderModule(gpu->device, module, NULL);
    return result == VK_SUCCESS || error(err, cap, "Cannot create authored compute pipelines");
}

static void stage_barrier(dc_gpu_t *gpu, bool host) {
    VkMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = host ? VK_PIPELINE_STAGE_2_HOST_BIT : VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = host ? VK_ACCESS_2_HOST_WRITE_BIT : VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                         VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_HOST_READ_BIT};
    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

bool fr_gpu_prepare_authored(dc_gpu_t *gpu, const fr_authored_request_t *r,
                             char *err, uint32_t cap) {
    if (!gpu || !fr_authored_request_valid(r, err, cap)) return false;
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "Cannot reach authored staging boundary");
    uint32_t count = r->width * r->height;
    struct dc_gpu_authored_stage *s = gpu->authored_stage;
    if (!s || s->pixels != count) {
        dc_gpu_authored_destroy(gpu);
        if (!stage_init(gpu, count, err, cap)) {dc_gpu_authored_destroy(gpu);return false;}
        s = gpu->authored_stage;
    }
    s->ready = false;
    dc_gpu_fluid_uploads_invalidate(gpu);
    memset(s->mapped, 0, STAGE_HEADER_WORDS * sizeof(uint32_t));
    s->mapped[FR_AUTHORED_TX_LO] = (uint32_t)r->transaction_id;
    s->mapped[FR_AUTHORED_TX_HI] = (uint32_t)(r->transaction_id >> 32);
    s->mapped[20] = UINT32_MAX;
    memcpy(s->mapped + STAGE_HEADER_WORDS, r->physical, r->physical_bytes);
    if (!gpu->fluid_work_possible) {
        const uint8_t *physical = r->physical;
        for (uint32_t i = 0; i < count; i++) {
            const uint8_t *pixel = physical + (size_t)i * FR_AUTHORED_PHYSICAL_STRIDE;
            if (pixel[0] == FR_PROP_MATERIAL_WATER && pixel[1] == 0u) {
                gpu->fluid_work_possible = true;
                break;
            }
        }
    }
    memcpy(s->mapped + STAGE_HEADER_WORDS + count * 2u, r->appearance, r->appearance_bytes);
    if (r->structure) {
        memcpy(s->mapped + STAGE_HEADER_WORDS + count * 18u, r->member_ownership, r->member_ownership_bytes);
        memcpy(s->mapped + STAGE_HEADER_WORDS + count * STAGE_WORDS_PER_PIXEL, r->structure, sizeof(*r->structure));
    }
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset authored command buffer");
    VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin authored command buffer");
    stage_barrier(gpu, true);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, s->layout, 0, 1, &s->descriptor, 0, NULL);
    uint32_t push[4] = {r->width, r->height, r->structure != NULL, 0};
    vkCmdPushConstants(gpu->command, s->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    for (uint32_t phase = 0; phase < STAGE_PHASES; phase++) {
        vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, s->pipelines[phase]);
        if (phase == 6u) {
            uint32_t parity = 0;
            for (uint32_t offset = 1; offset < count; offset *= 2u) {
                push[3] = offset * 2u + parity;
                vkCmdPushConstants(gpu->command, s->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
                vkCmdDispatch(gpu->command, (count + 127u) / 128u, 1, 1);
                stage_barrier(gpu, false);
                parity ^= 1u;
            }
            push[3] = parity;
            vkCmdPushConstants(gpu->command, s->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        } else {
            vkCmdDispatch(gpu->command, (count + 127u) / 128u, 1, 1);
            stage_barrier(gpu, false);
        }
    }
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end authored command buffer");
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command};
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS || vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "Cannot complete authored staging passes");
    s->ready = true;
    return true;
}

bool fr_gpu_read_authored_stage(dc_gpu_t *gpu,
                                fr_authored_stage_diagnostic_t *diagnostic,
                                uint32_t *labels, uint32_t capacity,
                                char *err, uint32_t cap) {
    struct dc_gpu_authored_stage *s = gpu ? gpu->authored_stage : NULL;
    if (!s || !s->ready || !diagnostic || (labels && capacity < s->pixels))
        return error(err, cap, "Invalid authored staging diagnostic request");
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "Cannot read completed authored staging");
    *diagnostic = (fr_authored_stage_diagnostic_t){s->mapped[2], s->mapped[4], s->mapped[16],
        s->mapped[17], s->mapped[18], s->mapped[19], s->mapped[20]};
    if (labels) memcpy(labels, s->mapped + STAGE_HEADER_WORDS + s->pixels * 19u, s->pixels * sizeof(*labels));
    return true;
}

bool fr_gpu_read_authored_components(dc_gpu_t *gpu,
                                     fr_authored_component_plan_t *components,
                                     uint32_t capacity, uint32_t *count,
                                     char *err_buf, uint32_t err_cap) {
    struct dc_gpu_authored_stage *s = gpu ? gpu->authored_stage : NULL;
    if (!s || !s->ready || !count)
        return error(err_buf, err_cap, "Invalid authored component readback");
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err_buf, err_cap, "Cannot read authored component plans");
    *count = s->mapped[4];
    if (s->mapped[2]) return error(err_buf, err_cap, "Authored staging contains invalid input");
    if (capacity < *count || (*count && !components))
        return error(err_buf, err_cap, "Authored component readback capacity is insufficient");
    size_t offset = STAGE_HEADER_WORDS + (size_t)s->pixels *
        (STAGE_WORDS_PER_PIXEL + STAGE_PLAN_PREFIX_WORDS) + DC_STRUCTURAL_GRAPH_WORDS;
    if (*count) memcpy(components, s->mapped + offset, (size_t)*count * sizeof(*components));
    return true;
}
