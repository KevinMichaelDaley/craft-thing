#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"
#include "../../src/vulkan/gpu_internal.h"

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
            ++g_fail;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() ++g_pass

static bool report_solver_profile(dc_gpu_t *gpu,unsigned side,const char *label) {
    uint64_t times[18],stages[4];
    if(vkGetQueryPoolResults(gpu->device,gpu->timestamp_pool,16,18,sizeof(times),times,
                            sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)!=VK_SUCCESS) return false;
    if(vkGetQueryPoolResults(gpu->device,gpu->timestamp_pool,12,4,sizeof(stages),stages,
                            sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)!=VK_SUCCESS) return false;
    double scale=gpu->timestamp_period/1e6,contacts=0;
    for(unsigned step=0;step<8;step++) contacts+=(times[3u+2u*step]-times[2u+2u*step])*scale;
    const uint32_t *pairs=dc_gpu_broadphase_data(gpu),*manifold=dc_gpu_contact_data(gpu);
    const dc_gpu_body_record_t *body=gpu->body_mapped;
    double hydro=(times[1]-times[0])*scale,solver=(stages[2]-stages[1])*scale;
    printf("%s ring%u prepare%.3fms hydro%.3fms contacts8steps%.3fms other-solver%.3fms raster/structure%.3fms "
           "pairs%u contacts%u visited%u boundary%u/%u "
           "ready%u mass%.6f inertia%.6f radius%.6f bounds(%d,%d)-(%d,%d) swept(%d,%d)-(%d,%d)\n",
           label,side,(stages[1]-stages[0])*scale,hydro,contacts,solver-hydro-contacts,(stages[3]-stages[2])*scale,
           pairs[0],manifold[0],manifold[9],manifold[10],manifold[11],
           body[0].motion_ready,body[0].mass,body[0].inertia,body[0].radius,
           body[0].bounds_low[0],body[0].bounds_low[1],body[0].bounds_high[0],body[0].bounds_high[1],
           body[0].swept_low[0],body[0].swept_low[1],body[0].swept_high[0],body[0].swept_high[1]);
    return true;
}

static void test_two_complex_pixel_bodies_meet_real_time_gpu_budget(void) {
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    ASSERT_TRUE(dc_gpu_create(&g, 480, 270, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned cy = 0; cy < 5; cy++)
        for (unsigned cx = 0; cx < 8; cx++) {
            *chunk = (dc_chunk_t){.coord = {cx, cy}};
            for (unsigned y = 0; y < 64; y++)
                for (unsigned x = 0; x < 64; x++) {
                    unsigned wx = cx * 64 + x, wy = cy * 64 + y,
                             material = DC_MATERIAL_AIR;
                    if (wy == 244 && wx < 480)
                        material = DC_MATERIAL_STONE;
                    else if (wx >= 180 && wx < 260 && wy >= 30 && wy < 94 &&
                             (wx < 186 || wx >= 254 || wy >= 88))
                        material = DC_MATERIAL_WOOD;
                    else if (wx >= 198 && wx < 270 && wy >= 115 && wy < 163 &&
                             (wx < 204 || (wy - 115) % 6 < 3))
                        material = wx < 210 ? DC_MATERIAL_STONE
                                            : DC_MATERIAL_DENSE_WOOD;
                    chunk->cells[y * 64 + x].material = material;
                }
            unsigned slot = cy * 8 + cx;
            ASSERT_TRUE(dc_gpu_upload_chunk(g, slot, chunk, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_set_page(g, cx, cy, slot, err, sizeof(err)));
        }
    free(chunk);
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_set_fluid_interval(g, 2));
    ASSERT_TRUE(dc_gpu_tick_step(g, err, sizeof(err)));
    ASSERT_TRUE(
        dc_gpu_set_body_motion(g, 0x80000001u, 0, .005f, 0, err, sizeof(err)));
    ASSERT_TRUE(
        dc_gpu_set_body_motion(g, 0x80000002u, 0, -.003f, 0, err, sizeof(err)));
    for (unsigned i = 0; i < 7; i++)
        ASSERT_TRUE(dc_gpu_tick_step(g, err, sizeof(err)));
    uint64_t rigid = 0, total = 0;
    for (unsigned i = 0; i < 24; i++) {
        dc_gpu_tick_capture_t capture;
        ASSERT_TRUE(dc_gpu_tick_capture(g, &capture, err, sizeof(err)));
        rigid += capture.stages[0].gpu_ns;
        for (unsigned stage = 0; stage < 3; stage++)
            total += capture.stages[stage].gpu_ns;
        dc_gpu_contact_stats_t stats;
        ASSERT_TRUE(dc_gpu_read_contacts(g, &stats, NULL, 0, err, sizeof(err)));
        ASSERT_EQ(stats.overflow, 0u);
    }
    dc_gpu_body_t first, second;
    ASSERT_TRUE(dc_gpu_read_body_id(g, 0x80000001u, &first, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(g, 0x80000002u, &second, err, sizeof(err)));
    ASSERT_TRUE(first.active && second.active);
    double rigid_ms = rigid / (24.0 * 1e6), total_ms = total / (24.0 * 1e6);
    printf("complex pixel mean GPU: rigid %.3f ms, whole tick %.3f ms\n",
           rigid_ms, total_ms);
    dc_gpu_destroy(g);
    ASSERT_TRUE(rigid_ms < 14.0);
    ASSERT_TRUE(total_ms < 1000.0 / 60.0);
    PASS();
}
static void test_cold_extraction_and_unchanged_complexity_sweep(void) {
    const unsigned sides[] = {32, 80, 160};
    int failures_before = g_fail;
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        char err[256] = {0};
        dc_gpu_t *g = NULL;
        ASSERT_TRUE(dc_gpu_create(
            &g, 480, 270, "build/shaders/pattern.comp.spv", err, sizeof(err)));
        dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
        ASSERT_TRUE(chunk);
        unsigned side = sides[scenario];
        for (unsigned cy = 0; cy < 5; ++cy)
            for (unsigned cx = 0; cx < 8; ++cx) {
                *chunk = (dc_chunk_t){.coord = {cx, cy}};
                for (unsigned y = 0; y < 64; ++y)
                    for (unsigned x = 0; x < 64; ++x) {
                        unsigned px = cx * 64 + x, py = cy * 64 + y;
                        if (px >= 100 && px < 100 + side && py >= 30 &&
                            py < 30 + side &&
                            (px == 100 || px == 99 + side || py == 30 ||
                             py == 29 + side))
                            chunk->cells[y * 64 + x].material =
                                px < 100 + side / 2 ? DC_MATERIAL_WOOD
                                                    : DC_MATERIAL_STONE;
                    }
                unsigned slot = cy * 8 + cx;
                ASSERT_TRUE(
                    dc_gpu_upload_chunk(g, slot, chunk, err, sizeof(err)));
                ASSERT_TRUE(dc_gpu_set_page(g, cx, cy, slot, err, sizeof(err)));
            }
        free(chunk);
        ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
        ASSERT_TRUE(dc_gpu_set_fluid_interval(g, 2));
        dc_gpu_tick_capture_t cold, warm;
        g->component_profile = true;
        ASSERT_TRUE(dc_gpu_tick_capture(g, &cold, err, sizeof(err)));
        uint64_t times[12], start_time;
        ASSERT_EQ(vkGetQueryPoolResults(
                      g->device, g->timestamp_pool, 0, 1, sizeof(start_time),
                      &start_time, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT),
                  VK_SUCCESS);
        ASSERT_EQ(vkGetQueryPoolResults(g->device, g->timestamp_pool, 4, 12,
                                        sizeof(times), times, sizeof(uint64_t),
                                        VK_QUERY_RESULT_64_BIT),
                  VK_SUCCESS);
        printf("before extraction %.3f ms\n",
               (times[0] - start_time) * g->timestamp_period / 1e6);
        for (unsigned stage = 0; stage < 11; ++stage)
            printf("extraction stage %u %.3f ms\n", stage,
                   (times[stage + 1] - times[stage]) * g->timestamp_period /
                       1e6);
        ASSERT_TRUE(report_solver_profile(g,side,"cold"));
        g->component_profile = false;
        uint32_t count;
        ASSERT_TRUE(dc_gpu_read_pixel_shape(g, 0x80000001u, NULL, 0, &count,
                                            err, sizeof(err)));
        ASSERT_EQ(count, 4 * side - 4);
        uint64_t unchanged = 0;
        for (unsigned tick = 0; tick < 8; ++tick) {
            ASSERT_TRUE(dc_gpu_tick_capture(g, &warm, err, sizeof(err)));
            unchanged += warm.stages[0].gpu_ns;
        }
        double cold_ms = cold.stages[0].gpu_ns / 1e6;
        double warm_ms = unchanged / 8e6;
        printf("ring %u pixels %u: cold rigid %.3f ms, unchanged %.3f ms\n",
               side, count, cold_ms, warm_ms);
        if(getenv("DC_COLD_DIAG")) {
            g->component_profile=true;
            dc_gpu_tick_capture_t diagnostic;
            ASSERT_TRUE(dc_gpu_tick_capture(g,&diagnostic,err,sizeof(err)));
            ASSERT_TRUE(report_solver_profile(g,side,"extra warm diagnostic"));
        }
        dc_gpu_destroy(g);
        if (!(cold_ms < 14.0)) {
            printf("FAIL cold ring%u: %.3fms must remain below14ms\n", side,
                   cold_ms);
            g_fail++;
        }
        if (!(warm_ms < 14.0)) {
            printf("FAIL unchanged ring%u: %.3fms must remain below14ms\n",
                   side, warm_ms);
            g_fail++;
        }
    }
    if (g_fail == failures_before)
        PASS();
}
int main(void) {
    RUN(test_cold_extraction_and_unchanged_complexity_sweep);
    RUN(test_two_complex_pixel_bodies_meet_real_time_gpu_budget);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
