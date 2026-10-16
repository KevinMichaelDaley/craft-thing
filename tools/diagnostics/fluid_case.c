#define main fluid_full_suite_main
#include "../../tests/vulkan/fluid_tests.c"
#undef main
#include <string.h>

static void test_completed_heads_match_closed_pool(void) {
    char err[256];
    dc_chunk_t left = {0}, right = {0};
    for (unsigned y = 10; y < 20; y++)
        for (unsigned x = 10; x < 20; x++)
            left.cells[y * 64 + x].fluid_mass = DC_FLUID_FULL;
    for (unsigned y = 9; y <= 20; y++) {
        left.cells[y * 64 + 9].material = DC_MATERIAL_STONE;
        left.cells[y * 64 + 20].material = DC_MATERIAL_STONE;
    }
    for (unsigned x = 9; x <= 20; x++) {
        left.cells[9 * 64 + x].material = DC_MATERIAL_STONE;
        left.cells[20 * 64 + x].material = DC_MATERIAL_STONE;
    }
    dc_gpu_t *g = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(g);
    VkBuffer out;
    VkDeviceMemory memory;
    uint32_t *words;
    unsigned n = g->width * g->height;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(g, n * 8u, &out, &memory,
                                          (void **)&words, err, sizeof(err)));
    ASSERT_EQ(vkResetCommandBuffer(g->command, 0), VK_SUCCESS);
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    ASSERT_EQ(vkBeginCommandBuffer(g->command, &begin), VK_SUCCESS);
    dc_gpu_record_fluid_phase(g, 0);
    VkMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                               .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                               .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT};
    vkCmdPipelineBarrier(g->command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0,
                         NULL, 0, NULL);
    VkBufferCopy copy = {.size = n * 4u};
    vkCmdCopyBuffer(g->command, g->fluid_b_buffer, out, 1, &copy);
    copy.dstOffset = n * 4u;
    vkCmdCopyBuffer(g->command, g->pressure_a_buffer, out, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(g->command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, NULL, 0,
                         NULL);
    ASSERT_EQ(vkEndCommandBuffer(g->command), VK_SUCCESS);
    VkFenceCreateInfo fence_info = {.sType =
                                        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence;
    ASSERT_EQ(vkCreateFence(g->device, &fence_info, NULL, &fence), VK_SUCCESS);
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &g->command};
    ASSERT_EQ(vkQueueSubmit(g->queue, 1, &submit, fence), VK_SUCCESS);
    ASSERT_EQ(
        vkWaitForFences(g->device, 1, &fence, VK_TRUE, UINT64_C(1000000000)),
        VK_SUCCESS);
    unsigned mismatches = 0;
    for (unsigned y = 0; y < g->height; y++)
        for (unsigned x = 0; x < g->width; x++) {
            unsigned expected = x >= 10 && x < 20 && y >= 10 && y < 20
                                    ? (y - 9u) * DC_FLUID_FULL
                                    : 0u,
                     index = y * g->width + x;
            if (words[index] != expected) {
                if (mismatches < 8)
                    printf("head %u,%u=%u expected%u pressure%.9g\n", x, y,
                           words[index], expected,
                           ((float *)(words + n))[index]);
                mismatches++;
            }
        }
    printf("completed heads mismatches%u; row10 head%u pressure%.9g\n",
           mismatches, words[10 * g->width + 10],
           ((float *)(words + n))[10 * g->width + 10]);
    vkDestroyFence(g->device, fence, NULL);
    vkUnmapMemory(g->device, memory);
    vkDestroyBuffer(g->device, out, NULL);
    vkFreeMemory(g->device, memory, NULL);
    dc_gpu_destroy(g);
    ASSERT_EQ(mismatches, 0u);
    PASS();
}
static void test_fluid_variant_fingerprint(void) {
    char err[256];
    dc_chunk_t left = {0}, right = {0};
    for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 64; x++) {
            left.cells[y * 64 + x].material =
                (x == 0 || y == 63) ? DC_MATERIAL_STONE : DC_MATERIAL_AIR;
            right.cells[y * 64 + x].material =
                (x == 63 || y == 63) ? DC_MATERIAL_STONE : DC_MATERIAL_AIR;
            if (y >= 28 && y < 60) {
                left.cells[y * 64 + x].fluid_mass =
                    (x > 3 && x < 63) ? DC_FLUID_FULL - (x * 73 + y * 97) % 8192
                                      : 0;
                right.cells[y * 64 + x].fluid_mass =
                    x < 60 ? DC_FLUID_FULL - (x * 101 + y * 41) % 8192 : 0;
            }
        }
    dc_gpu_t *g = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(g);
    const char *variant = getenv("DC_FLUID_VARIANTS");
    if (variant && strcmp(variant, "specialized") == 0) {
        for (unsigned p = 0; p < 13; p++)
            ASSERT_TRUE(g->fluid_phase_pipelines[p] != VK_NULL_HANDLE);
        for (unsigned p = 0; p < 6; p++)
            if (p != 4)
                ASSERT_TRUE(g->projection_phase_pipelines[p] != VK_NULL_HANDLE);
    }
    for (unsigned tick = 0; tick < 3; tick++)
        ASSERT_TRUE(dc_gpu_fluid_step(g, err, sizeof(err)));
    uint64_t hash = UINT64_C(14695981039346656037), mass = 0;
    dc_chunk_t saved;
    for (unsigned slot = 0; slot < 2; slot++) {
        ASSERT_TRUE(dc_gpu_download_chunk(g, slot, &saved, err, sizeof(err)));
        const unsigned char *bytes = (void *)saved.cells;
        for (size_t i = 0; i < sizeof(saved.cells); i++)
            hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
        bytes = (void *)saved.face_velocity;
        for (size_t i = 0; i < sizeof(saved.face_velocity); i++)
            hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
        for (unsigned i = 0; i < DC_CHUNK_CELLS; i++)
            mass += saved.cells[i].fluid_mass;
    }
    printf("fluid variant %s fingerprint %016llx mass %llu\n",
           variant ? variant : "production", (unsigned long long)hash,
           (unsigned long long)mass);
    dc_gpu_destroy(g);
    PASS();
}
static void test_long_transport_uses_exact_runtime_apertures(void) {
    char err[256];
    dc_chunk_t left={0},right={0},saved;
    for(unsigned y=0;y<64;y++) for(unsigned x=0;x<128;x++) {
        dc_chunk_t *chunk=x<64?&left:&right;
        unsigned i=y*64+x%64;
        chunk->cells[i].material=(x==0||x==127||y==63)?DC_MATERIAL_STONE:DC_MATERIAL_AIR;
        if(y>8&&y<55&&x>3&&x<124)
            chunk->cells[i].fluid_mass=DC_FLUID_FULL-(x*73+y*97)%8192;
        chunk->face_velocity[i].x=((int)(x%11)-5)*1.5f;
        chunk->face_velocity[i].y=((int)(y%7)-3)*.8f;
    }
    dc_gpu_t *g=make_grid(&left,&right,err,sizeof(err));ASSERT_TRUE(g);
    dc_gpu_body_t body={.id=99,.x_fp=48<<16,.y_fp=20<<16,.width=14,.height=12,.active=1};
    ASSERT_TRUE(dc_gpu_spawn_body(g,body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,99,.37f,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    memset(g->mpm_label_a_mapped,0xa5,2*128*64*sizeof(uint32_t));
    for(unsigned tick=0;tick<2;tick++) ASSERT_TRUE(dc_gpu_fluid_step(g,err,sizeof(err)));
    uint64_t hash=UINT64_C(14695981039346656037),mass=0;
    for(unsigned slot=0;slot<2;slot++) {
        ASSERT_TRUE(dc_gpu_download_chunk(g,slot,&saved,err,sizeof(err)));
        const unsigned char *bytes=(void *)saved.cells;
        for(size_t i=0;i<sizeof(saved.cells);i++) hash=(hash^bytes[i])*UINT64_C(1099511628211);
        bytes=(void *)saved.face_velocity;
        for(size_t i=0;i<sizeof(saved.face_velocity);i++) hash=(hash^bytes[i])*UINT64_C(1099511628211);
        for(unsigned i=0;i<4096;i++) mass+=saved.cells[i].fluid_mass;
    }
    const uint32_t *prefix=g->mpm_label_a_mapped,*coverage=g->occupancy_mapped;
    unsigned mismatches=0;
    for(unsigned y=0;y<64;y++) for(unsigned x=0;x<128;x++) {
        unsigned lo=x/16*16,hi=lo+16;
        uint32_t a=DC_FLUID_FULL,b=DC_FLUID_FULL;
        for(unsigned k=lo;k<=x;k++) {
            uint32_t covered=coverage[3*128*64+y*128+k];
            uint32_t value=DC_FLUID_FULL-(covered<DC_FLUID_FULL?covered:DC_FLUID_FULL);
            if(value<a) a=value;
        }
        for(unsigned k=x;k<hi;k++) {
            uint32_t covered=coverage[3*128*64+y*128+k];
            uint32_t value=DC_FLUID_FULL-(covered<DC_FLUID_FULL?covered:DC_FLUID_FULL);
            if(value<b) b=value;
        }
        mismatches+=prefix[2*(y*128+x)]!=a||prefix[2*(y*128+x)+1]!=b;
    }
    printf("long transport fingerprint %016llx mass%llu runtime aperture mismatches%u\n",
        (unsigned long long)hash,(unsigned long long)mass,mismatches);
    dc_gpu_destroy(g);
    ASSERT_EQ(mismatches,0u);PASS();
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    if (strcmp(argv[1], "aperture-runtime") == 0)
        RUN(test_long_transport_uses_exact_runtime_apertures);
    else if (strcmp(argv[1], "variant") == 0)
        RUN(test_fluid_variant_fingerprint);
    else if (strcmp(argv[1], "heads") == 0)
        RUN(test_completed_heads_match_closed_pool);
    else if (strcmp(argv[1], "displacement") == 0)
        RUN(test_rigid_displacement_raises_water_without_losing_volume);
    else if (strcmp(argv[1], "pressure") == 0)
        RUN(test_closed_liquid_velocity_is_projected);
    else if (strcmp(argv[1], "unknown") == 0)
        RUN(test_unloaded_neighbor_keeps_mass_in_source);
    else if (strcmp(argv[1], "phases") == 0)
        RUN(test_six_fluid_phases_match_one_uncoupled_update);
    else
        return 2;
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_pass != 1 || g_fail != 0;
}
