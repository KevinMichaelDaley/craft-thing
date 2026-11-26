#ifndef FERRUM_TESTS_FIRE_COVERAGE_CASES_H
#define FERRUM_TESTS_FIRE_COVERAGE_CASES_H

#include <float.h>

static void test_presented_coverage_thresholds_clip_and_count_overlap_once(void) {
    enum { width = 32u, height = 16u, y = 20u };
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_gas_state_t *input = calloc(DC_CHUNK_CELLS, sizeof(*input));
    double *volumes = malloc(DC_CHUNK_CELLS * sizeof(*volumes));
    ASSERT_TRUE(chunk && input && volumes);
    ASSERT_TRUE(context(&gpu, err, sizeof(err)) &&
                dc_gpu_set_viewport(gpu, 16u, 16u, width, height) &&
                dc_gpu_upload_chunk(gpu, 0u, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(gpu, 0u, 0u, 0u, err, sizeof(err)));
    fr_gas_model_t model = {.gas_constant = {.25984, .29680, .18892, .46152, 0},
        .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71}, .kinetic_scale = 1. / 512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(gpu, err, sizeof(err)) &&
                fr_gas_gpu_enable(gpu, &model, .0012f, 293.15f, err, sizeof(err)) &&
                fr_combustion_gpu_enable(gpu, err, sizeof(err)) &&
                fr_renderer_enable_fire_coverage(gpu, err, sizeof(err)));
    fr_renderer_fire_coverage_t counts;
    ASSERT_TRUE(!fr_renderer_read_fire_coverage(gpu, .2f, .1f, &counts));
    for (uint32_t cell = 0; cell < DC_CHUNK_CELLS; ++cell) volumes[cell] = 1.;
    const uint32_t positions[] = {10u, 20u, 30u, 32u};
    const double soot[] = {.0002, .0002, .0000005, .0000001};
    for (uint32_t i = 0; i < 4u; ++i) {
        fr_gas_state_t *gas = &input[y * DC_CHUNK_SIDE + positions[i]];
        gas->species[FR_GAS_OXYGEN] = .00028;
        gas->species[FR_GAS_NITROGEN] = .00072;
        gas->species[FR_GAS_SOOT] = soot[i];
        double temperature = i < 2u ? 1600. : 293.15;
        for (uint32_t species = 0; species < FR_GAS_SPECIES; ++species)
            gas->energy += gas->species[species] * model.heat_capacity_cv[species] * temperature;
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(gpu, 0u, input, volumes, DC_CHUNK_CELLS, err, sizeof(err)) &&
                fr_combustion_gpu_step(gpu, .001f, err, sizeof(err)) &&
                dc_gpu_present_chunks(gpu, err, sizeof(err)));
    ASSERT_TRUE(fr_renderer_read_fire_coverage(gpu, .2f, .1f, &counts));
    ASSERT_EQ(counts.flame_pixels, 0u);
    ASSERT_TRUE(fr_thermal_gpu_overlay(gpu, 0u) &&
                dc_gpu_present_chunks(gpu, err, sizeof(err)));
    ASSERT_TRUE(fr_renderer_read_fire_coverage(gpu, .2f, .1f, &counts));
    ASSERT_EQ(counts.viewport_pixels, width * height);
    ASSERT_EQ(counts.smoke_pixels, 2u);
    ASSERT_EQ(counts.flame_pixels, 1u);
    ASSERT_EQ(counts.covered_pixels, 2u);
    ASSERT_TRUE(fr_renderer_read_fire_coverage(gpu, .5f, FLT_MAX, &counts));
    ASSERT_EQ(counts.smoke_pixels, 1u);
    ASSERT_EQ(counts.flame_pixels, 0u);
    ASSERT_EQ(counts.covered_pixels, 1u);

    memset(input, 0, DC_CHUNK_CELLS * sizeof(*input));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(gpu, 0u, input, volumes, DC_CHUNK_CELLS, err, sizeof(err)) &&
                dc_gpu_present_chunks(gpu, err, sizeof(err)) &&
                fr_renderer_read_fire_coverage(gpu, .2f, .1f, &counts));
    ASSERT_EQ(counts.covered_pixels, 0u);
    free(chunk);
    free(input);
    free(volumes);
    dc_gpu_destroy(gpu);
    PASS();
}

#endif
