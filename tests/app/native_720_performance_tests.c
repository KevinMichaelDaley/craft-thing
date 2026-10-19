#define _POSIX_C_SOURCE 200809L
#define main pixel_component_fixture_main
#include "../vulkan/pixel_component_tests.c"
#undef main

#include <SDL.h>
#include <time.h>
#include "ferrum/renderer/gpu.h"
#include "../vulkan/native_scene.h"

#ifndef NATIVE_PERF_WIDTH
#define NATIVE_PERF_WIDTH 1280
#endif
#ifndef NATIVE_PERF_HEIGHT
#define NATIVE_PERF_HEIGHT 720
#endif
enum {
    WIDTH = NATIVE_PERF_WIDTH,
    HEIGHT = NATIVE_PERF_HEIGHT,
    SAMPLES = 30,
    SUSTAINED_SAMPLES = 180,
    PHYSICS_STEPS = 2,
    FLUID_CADENCE = 6
};
static double seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec / 1e9;
}
static int compare_sample(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static void profile_completed_concave_tick(dc_gpu_t *g) {
    uint64_t times[12];
    VkResult result = vkGetQueryPoolResults(
        g->device, g->timestamp_pool, 4, 12, sizeof(times), times,
        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    if (result != VK_SUCCESS) {
        printf("native profile queries status%d\n", result);
        return;
    }
    double scale = g->timestamp_period / 1e6;
    uint64_t solver_times[18];
    ASSERT_EQ(vkGetQueryPoolResults(g->device, g->timestamp_pool, 16, 18,
                                   sizeof(solver_times), solver_times,
                                   sizeof(uint64_t), VK_QUERY_RESULT_64_BIT),
              VK_SUCCESS);
    double contacts_ms = 0;
    for (unsigned step = 0; step < 8; step++)
        contacts_ms += (solver_times[3 + step * 2] -
                        solver_times[2 + step * 2]) * scale;
    printf("native final solver split: hydro%.3f contacts8substeps%.3f "
           "other%.3fms\n", (solver_times[1] - solver_times[0]) * scale,
           contacts_ms, (times[10] - times[9]) * scale - contacts_ms -
                        (solver_times[1] - solver_times[0]) * scale);
    printf("native final rigid detail: extraction%.3f prepare%.3f "
           "solver%.3f raster/contacts/structure%.3fms\n",
           (times[8] - times[0]) * scale, (times[9] - times[8]) * scale,
           (times[10] - times[9]) * scale, (times[11] - times[10]) * scale);
    const dc_gpu_body_record_t *b = g->body_mapped;
    unsigned active = 0, pixel = 0, ready = 0;
    for (unsigned i = 0; i < DC_GPU_BODY_CAPACITY; i++) {
        if (!b[i].body.active)
            continue;
        active++;
        pixel += (b[i].piece_count & DC_GPU_PIXEL_BODY_FLAG) != 0;
        ready += b[i].motion_ready != 0;
    }
    const uint32_t *contacts = dc_gpu_contact_data(g),
                   *pairs = dc_gpu_broadphase_data(g);
    printf("native final bodies active%u pixel%u cached%u; candidate "
           "pairs%u contacts%u overflow%u pixel queries%u boundaryA%u boundaryB%u\n",
           active, pixel, ready, pairs[0], contacts[0], contacts[2],
           contacts[9], contacts[10], contacts[11]);
}
static void native_scene(unsigned load) {
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    double began = seconds();
    ASSERT_TRUE(dc_gpu_create_window(&g, WIDTH, HEIGHT, WIDTH, HEIGHT,
                                     "build/shaders/pattern.comp.spv", err,
                                     sizeof(err)));
    ASSERT_EQ(g->width, WIDTH);
    ASSERT_EQ(g->height, HEIGHT);
    ASSERT_EQ(g->swap_extent.width, WIDTH);
    ASSERT_EQ(g->swap_extent.height, HEIGHT);
    ASSERT_TRUE(dc_gpu_set_viewport(g, 0, 0, WIDTH, HEIGHT));
    ASSERT_TRUE(dc_gpu_set_display_zoom(g, 1));
    ASSERT_TRUE(dc_gpu_set_fluid_interval(g, FLUID_CADENCE));
    ASSERT_TRUE(dc_gpu_set_tick_seconds(g, 1.f / 60.f));
    char title[160];
    snprintf(title, sizeof(title),
             "Native%d benchmark: %dx%d physics and display, scale1", HEIGHT,
             WIDTH, HEIGHT);
    ASSERT_TRUE(dc_gpu_set_window_title(g, title));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    unsigned columns = native_scene_columns(WIDTH),
             chunks = native_scene_count(WIDTH, HEIGHT);
    ASSERT_TRUE(chunks + 30u <= DC_GPU_CHUNK_SLOTS);
    const unsigned body_x = WIDTH / 2 - 120, body_y = HEIGHT / 6;
    for (unsigned slot = 0; slot < chunks; slot++) {
        native_scene_chunk(chunk, slot, WIDTH, HEIGHT);
        for (unsigned y = 0; y < 64; y++)
            for (unsigned x = 0; x < 64; x++) {
                unsigned px = (unsigned)chunk->coord.x * 64 + x,
                         py = (unsigned)chunk->coord.y * 64 + y;
                dc_cell_t *cell = &chunk->cells[y * 64 + x];
                if (load == 0) {
                    cell->fluid_mass = 0;
                    if (cell->material == DC_MATERIAL_SAND)
                        cell->material = DC_MATERIAL_AIR;
                }
                if (load == 2) {
                    if (px >= body_x && px < body_x + 80 && py >= body_y &&
                        py < body_y + 64 &&
                        (px < body_x + 6 || px >= body_x + 74 ||
                         py >= body_y + 58))
                        cell->material = DC_MATERIAL_WOOD;
                    if (px >= body_x + 18 && px < body_x + 90 &&
                        py >= body_y + 95 && py < body_y + 143 &&
                        (px < body_x + 24 || (py - body_y - 95) % 6 < 3))
                        cell->material = px < body_x + 30
                                             ? DC_MATERIAL_STONE
                                             : DC_MATERIAL_DENSE_WOOD;
                }
            }
        dc_chunk_seed_particles(chunk);
        ASSERT_TRUE(dc_gpu_upload_chunk(g, slot, chunk, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g, slot % columns, slot / columns, slot,
                                    err, sizeof(err)));
    }
    free(chunk);
    if (load == 2) {
        ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
        ASSERT_TRUE(dc_gpu_set_overlay(g, DC_GPU_OVERLAY_STRUCTURAL_STRESS));
    }
#ifdef DC_NATIVE_LIT_PERFORMANCE
    ASSERT_TRUE(dc_gpu_set_overlay(g, DC_GPU_OVERLAY_NONE));
    ASSERT_TRUE(fr_renderer_enable(g,err,sizeof(err)));
    fr_renderer_point_light_t lights[4];
    for(unsigned i=0;i<4;i++) lights[i]=(fr_renderer_point_light_t){
        .x=80.f+i*110.f,.y=45.f,.height=18.f,.radius=300.f,
        .color={i%3==0?1.f:.3f,i%3==1?1.f:.35f,i%3==2?1.f:.3f},.intensity=1800.f};
    fr_renderer_sun_t sun={.direction={-.3f,-.8f,.5f},.color={1.f,.94f,.8f},
        .intensity=1.4f,.known_sky=FR_SKY_TOP};
    ASSERT_TRUE(fr_renderer_set_sources(g,lights,4,sun));
#endif
    double loaded = seconds();
    printf("native load%u: submitting cold presented frame\n", load);
    if (load == 2 && getenv("DC_NATIVE_DIAG")) {
        double trace_start = seconds();
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
        printf("native diagnostic rigid-only step %.3fms\n",
               (seconds() - trace_start) * 1000.);
        dc_gpu_tick_capture_t trace;
        ASSERT_TRUE(dc_gpu_tick_capture(g, &trace, err, sizeof(err)));
        printf("native diagnostic tick rigid %.3f fluid %.3f MPM %.3fms\n",
               trace.stages[0].gpu_ns / 1e6, trace.stages[1].gpu_ns / 1e6,
               trace.stages[2].gpu_ns / 1e6);
        trace_start = seconds();
        ASSERT_TRUE(dc_gpu_present_chunks_steps(g, 0, err, sizeof(err)));
        printf("native diagnostic render-only %.3fms\n",
               (seconds() - trace_start) * 1000.);
        ASSERT_TRUE(dc_gpu_tick_capture(g, &trace, err, sizeof(err)));
        printf("native diagnostic second tick rigid %.3f fluid %.3f MPM "
               "%.3fms\n",
               trace.stages[0].gpu_ns / 1e6, trace.stages[1].gpu_ns / 1e6,
               trace.stages[2].gpu_ns / 1e6);
    }
    ASSERT_TRUE(
        dc_gpu_present_chunks_steps(g, PHYSICS_STEPS, err, sizeof(err)));
    ASSERT_EQ(vkQueueWaitIdle(g->queue), VK_SUCCESS);
    double cold_ms = (seconds() - loaded) * 1000.;
    printf("native load%u: cold presented frame completed %.3fms\n", load,
           cold_ms);
    if (load == 2) {
        uint32_t a = 0, b = 0;
        bool exact = dc_gpu_read_pixel_shape(g, 0x80000001u, NULL, 0, &a, err,
                                             sizeof(err)) &&
                     dc_gpu_read_pixel_shape(g, 0x80000002u, NULL, 0, &b, err,
                                             sizeof(err)) &&
                     a == 1176u && b == 1872u;
        if (!exact) {
            printf("native complex birth unavailable: %s\n", err);
            dc_gpu_destroy(g);
            ASSERT_TRUE(exact);
        }
        ASSERT_TRUE(dc_gpu_set_body_motion(g, 0x80000001u, 0, .005f, 0, err,
                                           sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_body_motion(g, 0x80000002u, 0, -.003f, 0, err,
                                           sizeof(err)));
    }
    for (unsigned i = 0; i < 8; i++) {
        ASSERT_TRUE(
            dc_gpu_present_chunks_steps(g, PHYSICS_STEPS, err, sizeof(err)));
        if (load == 2)
            printf("native concave warm frame%u complete\n", i);
    }
    unsigned sample_count = getenv("DC_NATIVE_SUSTAINED")
                                ? SUSTAINED_SAMPLES : SAMPLES;
    double samples[SUSTAINED_SAMPLES], start = seconds();
    for (unsigned i = 0; i < sample_count; i++) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            ASSERT_TRUE(event.type != SDL_QUIT);
        double tick = seconds();
        ASSERT_TRUE(
            dc_gpu_present_chunks_steps(g, PHYSICS_STEPS, err, sizeof(err)));
        samples[i] = (seconds() - tick) * 1000.;
    }
    ASSERT_EQ(vkQueueWaitIdle(g->queue), VK_SUCCESS);
    double wall_ms = (seconds() - start) * 1000. / sample_count;
    qsort(samples, sample_count, sizeof(*samples), compare_sample);
    uint64_t stage_ns[3] = {0};
    bool profile = load == 2 && getenv("DC_NATIVE_DIAG");
    g->component_profile = profile;
    for (unsigned i = 0; i < 6; i++) {
        dc_gpu_tick_capture_t capture;
        ASSERT_TRUE(dc_gpu_tick_capture(g, &capture, err, sizeof(err)));
        for (unsigned s = 0; s < 3; s++)
            stage_ns[s] += capture.stages[s].gpu_ns;
    }
    if (profile)
        profile_completed_concave_tick(g);
    g->component_profile = false;
    VkPhysicalDeviceProperties device;
    vkGetPhysicalDeviceProperties(g->physical, &device);
    printf("native load%u: physics=%ux%u display=%ux%u scale1 slots%u "
           "cadence%u,2fixed60Hzsteps/frame device=%s\n",
           load, g->width, g->height, g->swap_extent.width,
           g->swap_extent.height, DC_GPU_CHUNK_SLOTS, FLUID_CADENCE,
           device.deviceName);
    printf("samples %u setup %.3fs cold %.3fms frame mean %.3fms p95 %.3fms max %.3fms; "
           "offscreen1tick GPU rigid %.3f fluid %.3f MPM %.3fms\n",
           sample_count, loaded - began, cold_ms, wall_ms,
           samples[(sample_count * 95 + 99) / 100 - 1], samples[sample_count - 1],
           stage_ns[0] / 6e6, stage_ns[1] / 6e6, stage_ns[2] / 6e6);
    printf("presented FPS %.3f; fluid variants %s\n", 1000. / wall_ms,
           getenv("DC_FLUID_VARIANTS") ? getenv("DC_FLUID_VARIANTS")
                                       : "production");
    dc_gpu_destroy(g);
#ifdef DC_NATIVE_LIT_PERFORMANCE
    ASSERT_TRUE(wall_ms<30.);
    ASSERT_TRUE(samples[(sample_count*95+99)/100-1]<30.);
    ASSERT_TRUE(samples[sample_count-1]<30.);
#else
    ASSERT_TRUE(wall_ms <= 1000. / 30.);
    ASSERT_TRUE(samples[(sample_count * 95 + 99) / 100 - 1] <= 1000. / 30.);
#endif
    PASS();
}
static void test_native_dry_presented_frame_meets_30fps(void) {
    native_scene(0);
}
static void test_native_water_and_grains_presented_frame_meets_30fps(void) {
    native_scene(1);
}
static void test_native_exact_concave_bodies_presented_frame_meets_30fps(void) {
    native_scene(2);
}
int main(int argc, char **argv) {
    if (argc == 1 || strcmp(argv[1], "dry") == 0)
        RUN(test_native_dry_presented_frame_meets_30fps);
    if (argc == 1 || strcmp(argv[1], "dense") == 0)
        RUN(test_native_water_and_grains_presented_frame_meets_30fps);
    if (argc == 1 || strcmp(argv[1], "concave") == 0)
        RUN(test_native_exact_concave_bodies_presented_frame_meets_30fps);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
