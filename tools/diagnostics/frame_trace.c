/* Diagnostic-only displayed-frame timestamps; ordinary runtime has no hooks. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../src/vulkan/gpu_internal.h"

enum { QUERY_CAPACITY = 2048, RIGID = 0, FLUID = 1, MPM = 2,
       CONTACT = 3, HYDRO = 4, FLUID_PHASE = 9, RENDERER = 15,
       EXTRACTION = 16, BROADPHASE = 17, SOLVER = 18, LABEL_COUNT = 19 };
static dc_gpu_t *traced;
static VkQueryPool pool;
static unsigned query_count, frame;
static unsigned labels[QUERY_CAPACITY / 2];
static bool overflow;
static VkPipeline bound;

VkResult __real_vkBeginCommandBuffer(VkCommandBuffer,
                                     const VkCommandBufferBeginInfo *);
bool __real_dc_gpu_present_chunks_steps(dc_gpu_t *, uint32_t, char *, uint32_t);
void __real_dc_gpu_record_rigid(dc_gpu_t *);
void __real_dc_gpu_record_fluid_phase(dc_gpu_t *, uint32_t);
void __real_dc_gpu_record_mpm(dc_gpu_t *);
void __real_fr_renderer_record(dc_gpu_t *);
void __real_dc_gpu_record_contacts(dc_gpu_t *);
void __real_dc_gpu_record_component_extraction(dc_gpu_t *);
void __real_dc_gpu_record_broadphase(dc_gpu_t *);
void __real_dc_gpu_record_solver(dc_gpu_t *);
void __real_vkCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
void __real_vkCmdDispatch(VkCommandBuffer, uint32_t, uint32_t, uint32_t);
void __real_vkCmdDispatchIndirect(VkCommandBuffer, VkBuffer, VkDeviceSize);

static double seconds(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec + time.tv_nsec / 1e9;
}

static bool copy_activity(dc_gpu_t *gpu, VkBuffer source, VkBuffer destination,
                           VkDeviceSize bytes) {
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS ||
        vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return false;
    VkMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT};
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    VkBufferCopy copy = {.size = bytes};
    vkCmdCopyBuffer(gpu->command, source, destination, 1, &copy);
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return false;
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command};
    return vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
           vkQueueWaitIdle(gpu->queue) == VK_SUCCESS;
}

static bool profile_contact_pairs(dc_gpu_t *gpu) {
    size_t body_bytes = dc_gpu_body_storage_bytes(gpu);
    size_t activity_bytes = (size_t)gpu->mpm_scan_layout.words * sizeof(uint32_t);
    void *body = malloc(body_bytes);
    VkBuffer activity = VK_NULL_HANDLE;
    VkDeviceMemory activity_memory = VK_NULL_HANDLE;
    VkDeviceSize previous_device_bytes = gpu->device_only_bytes;
    char err[256];
    if (!body || !dc_gpu_make_device_buffer(gpu, activity_bytes, &activity,
                                            &activity_memory, err, sizeof(err))) {
        free(body);
        if (activity) vkDestroyBuffer(gpu->device, activity, NULL);
        if (activity_memory) vkFreeMemory(gpu->device, activity_memory, NULL);
        gpu->device_only_bytes = previous_device_bytes;
        return false;
    }
    memcpy(body, gpu->body_mapped, body_bytes);
    uint32_t *pairs = dc_gpu_broadphase_data(gpu);
    size_t pair_offset = (char *)pairs - (char *)gpu->body_mapped;
    const uint32_t *original = (const uint32_t *)((char *)body + pair_offset);
    unsigned count = original[0];
    unsigned base = DC_GPU_BROADPHASE_HEADER_WORDS +
                    2 * dc_gpu_broadphase_buckets(gpu);
    bool okay = count <= 64 && copy_activity(gpu, gpu->mpm_activity_buffer,
                                             activity, activity_bytes);
    for (unsigned i = 0; i < count && okay; i++) {
        double sum = 0;
        const uint32_t *pair = original + base + i * 8;
        for (unsigned sample = 0; sample < 4 && okay; sample++) {
            pairs[0] = 1;
            memcpy(pairs + base, pair, 8 * sizeof(uint32_t));
            VkCommandBufferBeginInfo begin = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            okay = vkResetCommandBuffer(gpu->command, 0) == VK_SUCCESS &&
                   vkBeginCommandBuffer(gpu->command, &begin) == VK_SUCCESS;
            if (!okay) break;
            vkCmdResetQueryPool(gpu->command, pool, 0, 2);
            vkCmdWriteTimestamp2(gpu->command,
                                 VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, pool, 0);
            dc_gpu_record_contacts(gpu);
            vkCmdWriteTimestamp2(gpu->command,
                                 VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, pool, 1);
            okay = vkEndCommandBuffer(gpu->command) == VK_SUCCESS;
            VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &gpu->command};
            okay = okay && vkQueueSubmit(gpu->queue, 1, &submit,
                                         VK_NULL_HANDLE) == VK_SUCCESS &&
                   vkQueueWaitIdle(gpu->queue) == VK_SUCCESS;
            uint64_t times[2];
            okay = okay && vkGetQueryPoolResults(
                gpu->device, pool, 0, 2, sizeof(times), times,
                sizeof(*times), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS;
            if (okay && sample)
                sum += (times[1] - times[0]) * gpu->timestamp_period / 1e6;
        }
        if (okay) {
            const uint32_t *contacts = dc_gpu_contact_data(gpu);
            printf("pair-trace,index%u,kind%u,bodyA%u,bodyB%u,"
                   "chunk(%u:%u,%u:%u),mean%.6f,contacts%u,visited%u\n",
                   i, pair[2], pair[0], pair[1], pair[5], pair[4], pair[7],
                   pair[6], sum / 3, contacts[0], contacts[9]);
        }
    }
    if (okay) {
        memcpy(gpu->body_mapped, body, body_bytes);
        okay = copy_activity(gpu, activity, gpu->mpm_activity_buffer,
                              activity_bytes);
    }
    free(body);
    vkDestroyBuffer(gpu->device, activity, NULL);
    vkFreeMemory(gpu->device, activity_memory, NULL);
    gpu->device_only_bytes = previous_device_bytes;
    return okay;
}

VkResult __wrap_vkBeginCommandBuffer(VkCommandBuffer command,
                                     const VkCommandBufferBeginInfo *begin) {
    VkResult result = __real_vkBeginCommandBuffer(command, begin);
    if (result == VK_SUCCESS && traced && command == traced->command) {
        query_count = 0;
        vkCmdResetQueryPool(command, pool, 0, QUERY_CAPACITY);
    }
    return result;
}

static void stage(dc_gpu_t *gpu, unsigned label, void (*record)(dc_gpu_t *)) {
    bool capture = gpu == traced && query_count + 2 <= QUERY_CAPACITY;
    unsigned at = query_count;
    if (gpu == traced && !capture)
        overflow = true;
    if (capture) {
        labels[at / 2] = label;
        query_count += 2;
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             pool, at);
    }
    record(gpu);
    if (capture)
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             pool, at + 1);
}

void __wrap_dc_gpu_record_rigid(dc_gpu_t *gpu) {
    stage(gpu, RIGID, __real_dc_gpu_record_rigid);
}
void __wrap_dc_gpu_record_fluid_phase(dc_gpu_t *gpu, uint32_t phase) {
    bool capture = gpu == traced && query_count + 2 <= QUERY_CAPACITY;
    unsigned at = query_count;
    if (gpu == traced && !capture)
        overflow = true;
    if (capture) {
        labels[at / 2] = FLUID_PHASE + phase;
        query_count += 2;
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             pool, at);
    }
    __real_dc_gpu_record_fluid_phase(gpu, phase);
    if (capture)
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             pool, at + 1);
}
void __wrap_dc_gpu_record_mpm(dc_gpu_t *gpu) {
    stage(gpu, MPM, __real_dc_gpu_record_mpm);
}
void __wrap_fr_renderer_record(dc_gpu_t *gpu) {
    stage(gpu, RENDERER, __real_fr_renderer_record);
}
void __wrap_dc_gpu_record_contacts(dc_gpu_t *gpu) {
    stage(gpu, CONTACT, __real_dc_gpu_record_contacts);
}
void __wrap_dc_gpu_record_component_extraction(dc_gpu_t *gpu) {
    stage(gpu, EXTRACTION, __real_dc_gpu_record_component_extraction);
}
void __wrap_dc_gpu_record_broadphase(dc_gpu_t *gpu) {
    stage(gpu, BROADPHASE, __real_dc_gpu_record_broadphase);
}
void __wrap_dc_gpu_record_solver(dc_gpu_t *gpu) {
    stage(gpu, SOLVER, __real_dc_gpu_record_solver);
}

void __wrap_vkCmdBindPipeline(VkCommandBuffer command, VkPipelineBindPoint point,
                              VkPipeline pipeline) {
    if (traced && command == traced->command &&
        point == VK_PIPELINE_BIND_POINT_COMPUTE)
        bound = pipeline;
    __real_vkCmdBindPipeline(command, point, pipeline);
}

static unsigned hydro_begin(VkCommandBuffer command) {
    if (!traced || command != traced->command)
        return QUERY_CAPACITY;
    for (unsigned phase = 0; phase < 5; phase++) {
        if (bound != traced->pixel_hydro_pipelines[phase])
            continue;
        if (query_count + 2 > QUERY_CAPACITY) {
            overflow = true;
            break;
        }
        unsigned at = query_count;
        labels[at / 2] = HYDRO + phase;
        query_count += 2;
        vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             pool, at);
        return at;
    }
    return QUERY_CAPACITY;
}

static void hydro_end(VkCommandBuffer command, unsigned at) {
    if (at != QUERY_CAPACITY)
        vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             pool, at + 1);
}

void __wrap_vkCmdDispatch(VkCommandBuffer command, uint32_t x, uint32_t y,
                          uint32_t z) {
    unsigned at = hydro_begin(command);
    __real_vkCmdDispatch(command, x, y, z);
    hydro_end(command, at);
}
void __wrap_vkCmdDispatchIndirect(VkCommandBuffer command, VkBuffer buffer,
                                  VkDeviceSize offset) {
    unsigned at = hydro_begin(command);
    __real_vkCmdDispatchIndirect(command, buffer, offset);
    hydro_end(command, at);
}

bool __wrap_dc_gpu_present_chunks_steps(dc_gpu_t *gpu, uint32_t steps,
                                       char *err, uint32_t cap) {
    VkQueryPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = QUERY_CAPACITY};
    if (vkCreateQueryPool(gpu->device, &info, NULL, &pool) != VK_SUCCESS)
        return false;
    traced = gpu;
    overflow = false;
    double began = seconds();
    bool okay = __real_dc_gpu_present_chunks_steps(gpu, steps, err, cap);
    double wall_ms = (seconds() - began) * 1000.;
    traced = NULL;
    uint64_t times[QUERY_CAPACITY];
    double total[LABEL_COUNT] = {0};
    if (okay && query_count) {
        okay = vkGetQueryPoolResults(gpu->device, pool, 0, query_count,
                                    query_count * sizeof(*times), times,
                                    sizeof(*times), VK_QUERY_RESULT_64_BIT)
                == VK_SUCCESS;
        if (okay)
            for (unsigned i = 0; i < query_count; i += 2)
                total[labels[i / 2]] += (times[i + 1] - times[i]) *
                                        gpu->timestamp_period / 1e6;
    }
    const uint32_t *contacts = dc_gpu_contact_data(gpu);
    const uint32_t *pairs = dc_gpu_broadphase_data(gpu);
    for (unsigned phase = 0; phase < 6; phase++)
        total[FLUID] += total[FLUID_PHASE + phase];
    unsigned number = frame++;
    printf("frame-trace,%u,steps%u,wall%.3f,rigid%.3f,fluid%.3f,mpm%.3f,"
           "pairs%u,contacts%u,visited%u,overflow%u,contactms%.3f,"
           "hydro0=%.3f,hydro1=%.3f,hydro2=%.3f,hydro3=%.3f,hydro4=%.3f,"
           "fluid0=%.3f,fluid1=%.3f,fluid2=%.3f,fluid3=%.3f,fluid4=%.3f,fluid5=%.3f,renderer=%.3f,extraction=%.3f,broadphase=%.3f,solver=%.3f\n", number, steps,
           wall_ms, total[RIGID], total[FLUID], total[MPM], pairs[0],
           contacts[0], contacts[9], contacts[2], total[CONTACT],
           total[HYDRO], total[HYDRO + 1], total[HYDRO + 2], total[HYDRO + 3], total[HYDRO + 4],
           total[FLUID_PHASE], total[FLUID_PHASE + 1], total[FLUID_PHASE + 2],
           total[FLUID_PHASE + 3], total[FLUID_PHASE + 4], total[FLUID_PHASE + 5],
           total[RENDERER],total[EXTRACTION],total[BROADPHASE],total[SOLVER]);
    if (okay && number == 72 && getenv("DC_CONTACT_PAIR_PROFILE"))
        okay = profile_contact_pairs(gpu);
    vkDestroyQueryPool(gpu->device, pool, NULL);
    return okay && !overflow;
}
