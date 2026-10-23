#define main bounded_probe_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main
#include "../../tools/diagnostics/shader_fixture.h"


#ifndef HYDRO_TEST_HEIGHT
#define HYDRO_TEST_HEIGHT 721
#endif
#ifndef HYDRO_TEST_SHADER
#define HYDRO_TEST_SHADER "build/shaders/hydro_head.comp.spv"
#endif
enum {
    FULL = 65536,
    CAP = 64 * FULL,
    WIDTH = 64,
    HEIGHT = HYDRO_TEST_HEIGHT,
    PAGES = (HEIGHT + 63) / 64,
    CELLS = WIDTH * HEIGHT
};
static int g_pass, g_fail;
#define RUN(fn)                                                                \
    do {                                                                       \
        printf("RUN %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            printf("FAIL %d: %s\n", __LINE__, #e);                             \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++
typedef struct {
    probe_t p;
    dc_gpu_t gpu;
    VkBuffer buffers[6];
    VkDeviceMemory memories[6];
    uint32_t *words[6];
    uint32_t expected[CELLS];
} fixture_t;
static uint32_t capped(uint32_t a, uint32_t b) {
    return a + b < CAP ? a + b : CAP;
}
static uint32_t atlas(const fixture_t *f, int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
        return UINT32_MAX;
    unsigned page = f->words[1][y / 64];
    return page ? (page - 1) * 4096 + (unsigned)(y % 64) * 64 + (unsigned)x
                : UINT32_MAX;
}
static bool open_cell(const fixture_t *f, int x, int y) {
    uint32_t a = atlas(f, x, y);
    if (a == UINT32_MAX)
        return false;
    unsigned m = f->words[0][2 * a] & 65535;
    return (m == 0 || m == 2 || m == 4 || m == 5 || m == 6) &&
           f->words[2][2 * CELLS + y * WIDTH + x] < FULL;
}
static bool wet(const fixture_t *f, int x, int y) {
    return open_cell(f, x, y) && f->words[0][2 * atlas(f, x, y) + 1] != 0;
}
static uint32_t neighboring(const fixture_t *f, int x, int y) {
    uint32_t result = 0;
    for (int side = -1; side <= 1; side += 2)
        for (int d = 1; d <= 32; d++) {
            int q = x + side * d;
            uint32_t a = atlas(f, q, y);
            if (a == UINT32_MAX)
                break;
            uint32_t m = f->words[0][2 * a] & 65535;
            if (m != 0 && m != 2 && m != 4 && m != 5 && m != 6)
                break;
            if (f->words[2][y * WIDTH + q])
                continue;
            uint32_t head = 0;
            for (int above = y; above >= 0; above--) {
                if (!wet(f, q, above))
                    break;
                head = capped(head, f->words[0][2 * atlas(f, q, above) + 1]);
            }
            if (head > result)
                result = head;
            break;
        }
    return result;
}
static void reference(fixture_t *f) {
    for (int x = 0; x < WIDTH; x++) {
        uint32_t head = 0;
        for (int y = 0; y < HEIGHT; y++) {
            uint32_t i = y * WIDTH + x, covered = f->words[2][2 * CELLS + i];
            if (covered) {
                uint32_t water = f->words[0][2 * atlas(f, x, y) + 1];
                uint32_t increment =
                    water + covered < FULL ? water + covered : FULL;
                head = head ? capped(head, increment) : neighboring(f, x, y);
            } else
                head = wet(f, x, y)
                           ? capped(head, f->words[0][2 * atlas(f, x, y) + 1])
                           : 0;
            f->expected[i] = head;
        }
    }
}
static bool setup(fixture_t *f, unsigned scenario) {
    if (!initialize(&f->p))
        return false;
    f->gpu = (dc_gpu_t){.device = f->p.device, .physical = f->p.physical};
    const uint32_t counts[] = {PAGES * 4096u * 2u, PAGES, 5u * CELLS, CELLS, CELLS, 1};
    const uint32_t bindings[] = {1, 2, 3, 9, 11, 31};
    char err[256];
    for (unsigned i = 0; i < 6; i++) {
        if (!dc_gpu_make_mapped_buffer(&f->gpu, counts[i] * sizeof(uint32_t),
                                       &f->buffers[i], &f->memories[i],
                                       (void **)&f->words[i], err, sizeof(err)))
            return false;
        memset(f->words[i], 0, counts[i] * sizeof(uint32_t));
    }
    f->p.buffer = f->buffers[3];
    f->p.mapped = f->words[3];
    if (!fixture_body_pipeline(&f->p, &f->gpu, VK_NULL_HANDLE,
                               HYDRO_TEST_SHADER, bindings, 6,
                               UINT32_MAX))
        return false;
    for (unsigned i = 0; i < 6; i++) {
        VkDescriptorBufferInfo info = {f->buffers[i], 0,
                                       counts[i] * sizeof(uint32_t)};
        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = f->p.descriptor,
            .dstBinding = bindings[i],
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &info};
        vkUpdateDescriptorSets(f->p.device, 1, &write, 0, NULL);
    }
    for (unsigned p = 0; p < PAGES; p++)
        f->words[1][p] = p + 1;
    if (scenario == 2)
        f->words[1][3] = 0;
    for (unsigned i = 0; i < CELLS; i++)
        f->words[3][i] = UINT32_MAX;
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++) {
            uint32_t a = atlas(f, x, y), i = y * WIDTH + x;
            if (a == UINT32_MAX)
                continue;
            unsigned h = (unsigned)(x * 1664525u + y * 1013904223u);
            f->words[0][2 * a] = (x == 30 && y % 23 == 0) ? 1
                                 : h % 7 == 0             ? 2
                                                          : 0;
            f->words[0][2 * a + 1] = y > 200 || h % 5 ? FULL - h % FULL : 0;
            if (scenario && (x > 10 && x < 25) && y % 37 < 17) {
                f->words[2][i] = 19;
                f->words[2][2 * CELLS + i] = y % 3 ? FULL : FULL / 3;
            }
        }
    reference(f);
    return true;
}
static void run_case(unsigned scenario) {
    fixture_t *f = calloc(1, sizeof(*f));
    ASSERT_TRUE(f);
    ASSERT_TRUE(setup(f, scenario));
    probe_t *p = &f->p;
    ASSERT_TRUE(begin(p));
    vkCmdBindPipeline(p->command, VK_PIPELINE_BIND_POINT_COMPUTE, p->pipeline);
    vkCmdBindDescriptorSets(p->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                            p->layout, 0, 1, &p->descriptor, 0, NULL);
    for (unsigned phase = 0; phase < 2; phase++) {
        uint32_t push[8] = {WIDTH, HEIGHT, phase};
        vkCmdPushConstants(p->command, p->layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(push), push);
        vkCmdDispatch(p->command, WIDTH, 1, 1);
        VkMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                   .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                   .dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                                    VK_ACCESS_SHADER_WRITE_BIT};
        vkCmdPipelineBarrier(p->command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &barrier, 0, NULL, 0, NULL);
    }
    host_barrier(p, 1);
    ASSERT_TRUE(check(vkEndCommandBuffer(p->command), "head command end"));
    ASSERT_TRUE(submit(p, "parallel hydro head", 1));
    unsigned mismatches = 0;
    for (unsigned i = 0; i < CELLS; i++)
        if (f->words[3][i] != f->expected[i]) {
            if (!mismatches)
                printf("scenario%u first mismatch%u: %u expected%u\n", scenario,
                       i, f->words[3][i], f->expected[i]);
            mismatches++;
        }
    printf("scenario%u %u exact heads, %u mismatches\n", scenario, CELLS,
           mismatches);
#ifdef HYDRO_TEST_PROFILE
    uint32_t evaluations=f->words[5][0];
    uint32_t budget=2u*CELLS*((HEIGHT+127u)/128u<=8u?1u:2u);
    printf("transform evaluations%u budget%u\n",evaluations,budget);
    if(evaluations>budget) mismatches++;
#endif
    fixture_release_pipeline(p);
    for (unsigned i = 0; i < 6; i++) {
        vkUnmapMemory(p->device, f->memories[i]);
        vkDestroyBuffer(p->device, f->buffers[i], NULL);
        vkFreeMemory(p->device, f->memories[i], NULL);
    }
    vkDestroyFence(p->device, p->fence, NULL);
    vkDestroyCommandPool(p->device, p->pool, NULL);
    vkDestroyDevice(p->device, NULL);
    vkDestroyInstance(p->instance, NULL);
    free(f);
    ASSERT_EQ(mismatches, 0);
    PASS();
}
static void test_partial_tall_column_matches_serial_recurrence(void) {
    run_case(0);
}
static void test_fractional_coverage_and_lateral_fallback_match(void) {
    run_case(1);
}
static void test_unknown_chunks_break_water_columns_conservatively(void) {
    run_case(2);
}
int main(void) {
    RUN(test_partial_tall_column_matches_serial_recurrence);
    RUN(test_fractional_coverage_and_lateral_fallback_match);
    RUN(test_unknown_chunks_break_water_columns_conservatively);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
