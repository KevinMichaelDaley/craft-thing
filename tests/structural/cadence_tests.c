#define DC_STRUCTURAL_PRESENTATION_TEST
#define main placement_fixture_main
#include "placement_tests.c"
#undef main
#include <SDL2/SDL.h>

static bool check_cadence(unsigned batch, unsigned idle_frames) {
    char err[256] = {0}, title[128];
    dc_gpu_t *g = grid();
    if (!g)
        return false;
    snprintf(title, sizeof(title),
             "Structural grace: %u physics ticks per presented frame", batch);
    dc_gpu_set_window_title(g, title);
    fr_structural_gpu_graph_t s = beam(20);
    s.anchor_count = 0;
    bool ok = fr_gpu_place_structure(g, &s, err, sizeof(err));
    unsigned ticks = 0;
    uint64_t begin = SDL_GetPerformanceCounter();
    while (ok && ticks < 89) {
        unsigned count = batch < 89 - ticks ? batch : 89 - ticks;
        ok = dc_gpu_present_chunks_steps(g, count, err, sizeof(err));
        ticks += count;
        for (unsigned i = 0; ok && i < idle_frames; i++)
            ok = dc_gpu_present_chunks_steps(g, 0, err, sizeof(err));
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
        }
        SDL_Delay(2);
    }
    dc_gpu_body_motion_t motion;
    if (ok)
        ok = dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &motion,
                                     err, sizeof(err)) &&
             (motion.flags & DC_GPU_BODY_KINEMATIC) != 0u;
    for (unsigned i = 0; ok && i < 8; i++)
        ok = dc_gpu_present_chunks_steps(g, 0, err, sizeof(err));
    if (ok)
        ok = dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &motion,
                                     err, sizeof(err)) &&
             (motion.flags & DC_GPU_BODY_KINEMATIC) != 0u;
    if (ok)
        ok = dc_gpu_present_chunks_steps(g, 1, err, sizeof(err));
    if (ok)
        ok = dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &motion,
                                     err, sizeof(err)) &&
             (motion.flags & DC_GPU_BODY_KINEMATIC) == 0u;
    double wall = (double)(SDL_GetPerformanceCounter() - begin) /
                  SDL_GetPerformanceFrequency();
    printf("cadence batch %u, idle %u: conversion at 90 ticks / 1.500 "
           "simulated seconds, wall %.3f s\n",
           batch, idle_frames, wall);
    if (!ok)
        printf("cadence failed: %s\n", err);
    dc_gpu_destroy(g);
    return ok;
}
static void
test_presented_frame_cadence_and_idle_frames_do_not_change_grace(void) {
    ASSERT_TRUE(check_cadence(1, 2));
    ASSERT_TRUE(check_cadence(5, 0));
    PASS();
}
int main(void) {
    RUN(test_presented_frame_cadence_and_idle_frames_do_not_change_grace);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
