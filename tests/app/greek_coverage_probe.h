#ifndef FERRUM_TESTS_GREEK_COVERAGE_PROBE_H
#define FERRUM_TESTS_GREEK_COVERAGE_PROBE_H

#include <inttypes.h>

#include "ferrum/renderer/gpu.h"

typedef struct {
    fr_gas_gpu_clock_t start;
    float smoke_alpha, flame_emission;
    uint32_t tick;
} greek_coverage_probe_t;

static bool greek_coverage_sample(dc_gpu_t *gpu, const greek_coverage_probe_t *probe) {
    fr_renderer_fire_coverage_t counts;
    fr_gas_gpu_clock_t clock;
    if (!fr_renderer_read_fire_coverage(gpu, probe->smoke_alpha, probe->flame_emission,
                                        &counts) ||
        !fr_gas_gpu_clock(gpu, &clock))
        return false;
    printf("COVERAGE_FRAME %u %" PRIu64 " %.9f %u %u %u %u\n",
           probe->tick, clock.physical_steps - probe->start.physical_steps,
           clock.advanced_seconds - probe->start.advanced_seconds, counts.viewport_pixels,
           counts.smoke_pixels, counts.flame_pixels, counts.covered_pixels);
    fflush(stdout);
    return true;
}

static bool greek_coverage_advance(dc_gpu_t *gpu, void *sample, char *err, uint32_t cap) {
    greek_coverage_probe_t *probe = sample;
    if (greek_stopped || !dc_gpu_present_chunks_steps(gpu, 1u, err, cap)) return false;
    ++probe->tick;
    return greek_coverage_sample(gpu, probe);
}

static bool greek_parse_coverage_threshold(const char *text, float *out) {
    char *end;
    float value = strtof(text, &end);
    if (end == text || *end || !isfinite(value) || value <= 0.f) return false;
    *out = value;
    return true;
}

static int greek_coverage_record(int argc, char **argv) {
    uint32_t seed, ticks;
    greek_coverage_probe_t probe = {0};
    if (argc != 6 || !greek_parse_u32(argv[2], &seed) ||
        !greek_parse_u32(argv[3], &ticks) || ticks == 0u ||
        !greek_parse_coverage_threshold(argv[4], &probe.smoke_alpha) ||
        probe.smoke_alpha > 1.f ||
        !greek_parse_coverage_threshold(argv[5], &probe.flame_emission)) {
        fprintf(stderr, "Expected a seed, tick count and visible coverage thresholds\n");
        return 2;
    }
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_siege_scene_t scene = {0};
    bool okay = dc_gpu_create_window(&gpu, SIM_WIDTH, SIM_HEIGHT,
        VIEW_WIDTH * WINDOW_SCALE, VIEW_HEIGHT * WINDOW_SCALE,
        "build/shaders/pattern.comp.spv", err, sizeof(err)) &&
        dc_gpu_set_viewport(gpu, 0u, 0u, VIEW_WIDTH, VIEW_HEIGHT) &&
        dc_gpu_set_display_zoom(gpu, WINDOW_SCALE) &&
        dc_gpu_set_tick_seconds(gpu, 1.f / 60.f) &&
        dc_siege_scene_build_risk(gpu, DC_SIEGE_LOAD_GREEK, seed, false,
                                  &scene, err, sizeof(err)) &&
        fr_renderer_enable_fire_coverage(gpu, err, sizeof(err)) &&
        fr_renderer_set_mode(gpu, FR_RENDERER_LIT) &&
        greek_ticks(gpu, 2u, err, sizeof(err)) &&
        fr_gas_gpu_clock(gpu, &probe.start) &&
        dc_siege_scene_release(gpu, &scene, err, sizeof(err)) &&
        dc_siege_scene_ignite_load(gpu, &scene, greek_prompt_fuse_seconds,
                                   err, sizeof(err)) &&
        dc_gpu_present_chunks(gpu, err, sizeof(err)) &&
        greek_coverage_sample(gpu, &probe) &&
        greek_advance_to_pouch_release(gpu, &scene, greek_coverage_advance, &probe,
                                       err, sizeof(err));
    dc_gpu_world_body_t released, final;
    bool attached = true;
    if (okay) okay = fr_gpu_revolute_active(gpu, scene.pouch_joint, &attached,
                                            err, sizeof(err)) && !attached &&
        dc_gpu_read_world_body(gpu, scene.coal_body, &released, err, sizeof(err));
    if (okay) printf("COVERAGE_RELEASE %u\n", probe.tick);
    for (uint32_t tick = 0u; okay && tick < ticks; ++tick)
        okay = greek_coverage_advance(gpu, &probe, err, sizeof(err));
    if (okay) okay = dc_gpu_read_world_body(gpu, scene.coal_body, &final, err, sizeof(err)) &&
        (released.chunk.x != final.chunk.x || released.chunk.y != final.chunk.y ||
         released.body.x_fp != final.body.x_fp || released.body.y_fp != final.body.y_fp);
    if (!okay) fprintf(stderr, "Displayed prompt shot did not complete. %s\n", err);
    dc_gpu_destroy(gpu);
    return okay ? 0 : 1;
}

#endif
