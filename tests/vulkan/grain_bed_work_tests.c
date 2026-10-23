#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
static char g_error[256];
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %d: %s (%s)\n", __LINE__, #e, g_error); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

static bool triplet(dc_gpu_t *gpu, uint32_t word, uint32_t output[3]) {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = NULL;
    uint64_t local = gpu->mapped_local_bytes, system = gpu->mapped_system_bytes;
    if (!dc_gpu_make_mapped_buffer(gpu, 12, &buffer, &memory, &mapped, g_error, sizeof(g_error))) return false;
    VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bool okay = vkResetCommandBuffer(gpu->command, 0) == VK_SUCCESS &&
        vkBeginCommandBuffer(gpu->command, &begin) == VK_SUCCESS;
    if (okay) {
        VkMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT, .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT};
        VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(gpu->command, &dependency);
        VkBufferCopy copy = {(VkDeviceSize)word * 4u, 0, 12};
        vkCmdCopyBuffer(gpu->command, gpu->mpm_activity_buffer, buffer, 1, &copy);
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
        vkCmdPipelineBarrier2(gpu->command, &dependency);
        okay = vkEndCommandBuffer(gpu->command) == VK_SUCCESS;
    }
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command};
    if (okay) okay = vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
        vkQueueWaitIdle(gpu->queue) == VK_SUCCESS;
    if (okay) memcpy(output, mapped, 12);
    vkUnmapMemory(gpu->device, memory);
    vkDestroyBuffer(gpu->device, buffer, NULL);
    vkFreeMemory(gpu->device, memory, NULL);
    gpu->mapped_local_bytes = local; gpu->mapped_system_bytes = system;
    return okay;
}

static dc_gpu_t *context(bool grains) {
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    bool okay = chunk && dc_gpu_create(&gpu, 64, 64,
        "build/shaders/pattern.comp.spv", g_error, sizeof(g_error));
    if (grains && chunk) chunk->cells[50 * 64 + 40].material = DC_MATERIAL_DIRT;
    if (okay) okay = dc_gpu_upload_chunk(gpu, 0, chunk, g_error, sizeof(g_error)) &&
        dc_gpu_set_page(gpu, 0, 0, 0, g_error, sizeof(g_error)) &&
        dc_gpu_enable_automatic_components(gpu);
    free(chunk);
    if (!okay) { dc_gpu_destroy(gpu); return NULL; }
    return gpu;
}

static bool spawn(dc_gpu_t *gpu) {
    dc_gpu_body_t body = {.x_fp = 8 * 65536, .y_fp = 8 * 65536,
        .width = 4, .height = 4, .id = 1, .active = 1};
    return dc_gpu_spawn_body(gpu, body, g_error, sizeof(g_error));
}

static void test_body_without_particles_has_no_grain_bed_work(void) {
    dc_gpu_t *gpu = context(false); ASSERT_TRUE(gpu);
    ASSERT_TRUE(spawn(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, g_error, sizeof(g_error)));
    uint32_t work[3]; ASSERT_TRUE(triplet(gpu, DC_GRAIN_BED_WORK_WORD, work));
    ASSERT_EQ(work[0], 0u); ASSERT_EQ(work[1], 1u); ASSERT_EQ(work[2], 1u);
    ASSERT_TRUE(triplet(gpu, DC_RIGID_BODY_WORK_WORD, work)); ASSERT_EQ(work[0], 1u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_grain_bed_work_tracks_body_activation_on_gpu(void) {
    dc_gpu_t *gpu = context(true); ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, g_error, sizeof(g_error)));
    uint32_t work[3]; ASSERT_TRUE(triplet(gpu, DC_GRAIN_BED_WORK_WORD, work));
    ASSERT_EQ(work[0], 0u); ASSERT_EQ(work[1], 1u); ASSERT_EQ(work[2], 1u);
    ASSERT_TRUE(triplet(gpu, DC_RIGID_BODY_WORK_WORD, work)); ASSERT_EQ(work[0], 0u);
    ASSERT_TRUE(spawn(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, g_error, sizeof(g_error)));
    ASSERT_TRUE(triplet(gpu, DC_GRAIN_BED_WORK_WORD, work));
    ASSERT_EQ(work[0], 1u); ASSERT_EQ(work[1], 1u); ASSERT_EQ(work[2], 1u);
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 1, g_error, sizeof(g_error)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, g_error, sizeof(g_error)));
    ASSERT_TRUE(triplet(gpu, DC_GRAIN_BED_WORK_WORD, work)); ASSERT_EQ(work[0], 0u);
    dc_gpu_destroy(gpu); PASS();
}


static bool bed_reference_pipelines(dc_gpu_t *g) {
    VkShaderModule module=VK_NULL_HANDLE;
    if(!dc_gpu_load_shader_module(g,"build/shaders/rigid_solver.comp.spv",&module,g_error,sizeof(g_error))) return false;
    const uint32_t phases[]={18,19,21};
    VkSpecializationMapEntry entries[]={{0,0,4},{2,4,4}};
    bool okay=true;
    for(unsigned i=0;i<3;i++) {
        uint32_t data[]={phases[i],0};
        VkSpecializationInfo spec={2,entries,sizeof(data),data};
        VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
                .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=g->pipeline_layout};
        VkPipeline pipeline=VK_NULL_HANDLE;
        if(vkCreateComputePipelines(g->device,VK_NULL_HANDLE,1,&info,NULL,&pipeline)!=VK_SUCCESS) {okay=false;break;}
        vkDestroyPipeline(g->device,g->solver_pipelines[phases[i]],NULL);
        g->solver_pipelines[phases[i]]=pipeline;
    }
    vkDestroyShaderModule(g->device,module,NULL);g->solver_cache_disabled=true;return okay;
}
static void test_compact_bed_preserves_every_support_graph_value(void) {
    float expected[64u*64u*4u];
    for(unsigned compact=0;compact<2;compact++) {
        dc_gpu_t *g=context(false);ASSERT_TRUE(g);
        dc_chunk_t chunk={0};
        for(unsigned y=12;y<61;y++) for(unsigned x=7;x<54;x++)
            if((x*13u+y*7u)%29u!=0u) chunk.cells[y*64u+x].material=DC_MATERIAL_DIRT;
        for(unsigned x=0;x<64;x++) chunk.cells[61u*64u+x].material=DC_MATERIAL_STONE;
        ASSERT_TRUE(dc_gpu_upload_chunk(g,0,&chunk,g_error,sizeof(g_error)));
        if(!compact) ASSERT_TRUE(bed_reference_pipelines(g));
        ASSERT_TRUE(spawn(g));ASSERT_TRUE(dc_gpu_rigid_step(g,g_error,sizeof(g_error)));
        if(!compact) memcpy(expected,g->mpm_grid_mapped,sizeof(expected));
        else {
            unsigned mismatches=0;
            const float *actual=g->mpm_grid_mapped;
            for(unsigned i=0;i<64u*64u*4u;i++) mismatches+=memcmp(expected+i,actual+i,4u)!=0;
            printf("compact support graph mismatches%u\n",mismatches);
            ASSERT_EQ(mismatches,0u);
        }
        dc_gpu_destroy(g);
    }
    PASS();
}
int main(void) {
    RUN(test_body_without_particles_has_no_grain_bed_work);
    RUN(test_grain_bed_work_tracks_body_activation_on_gpu);
    RUN(test_compact_bed_preserves_every_support_graph_value);
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
