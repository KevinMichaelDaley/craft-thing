#ifndef FERRUM_TESTS_GAS_AUDIT_PRECISION_CASES_H
#define FERRUM_TESTS_GAS_AUDIT_PRECISION_CASES_H

static void test_cold_audit_keeps_trace_products_across_workgroups(void) {
    enum { tiles = 10u };
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1u, sizeof(*chunk));
    fr_gas_state_t *gas = calloc(DC_CHUNK_CELLS, sizeof(*gas));
    double *volume = malloc(DC_CHUNK_CELLS * sizeof(*volume));
    ASSERT_TRUE(chunk && gas && volume);
    ASSERT_TRUE(dc_gpu_create(&gpu, tiles * DC_CHUNK_SIDE, DC_CHUNK_SIDE,
        "build/shaders/pattern.comp.spv", err, sizeof(err)));
    for (uint32_t tile = 0u; tile < tiles; ++tile) {
        chunk->coord.x = tile;
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu, tile, chunk, err, sizeof(err)) &&
                    dc_gpu_set_page(gpu, tile, 0u, tile, err, sizeof(err)));
    }
    fr_gas_model_t model = reference_model();
    ASSERT_TRUE(fr_gas_gpu_enable(gpu, &model, 1.f, 300.f, err, sizeof(err)));
    for (uint32_t cell = 0; cell < DC_CHUNK_CELLS; ++cell) {
        double stock = cell % DC_CHUNK_SIDE == 0u ? 1. : 0x1p-27;
        gas[cell].species[FR_GAS_EXHAUST] = stock;
        gas[cell].energy = stock * model.heat_capacity_cv[FR_GAS_EXHAUST] * 300.;
        volume[cell] = (cell / DC_CHUNK_SIDE) % 2u ? 1. : 0.;
    }
    for (uint32_t tile = 0u; tile < tiles; ++tile)
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(gpu, tile, gas, volume, DC_CHUNK_CELLS,
                                          err, sizeof(err)));

    fr_gas_gpu_audit_t audit;
    fr_gas_gpu_clock_t before, after;
    ASSERT_TRUE(fr_gas_gpu_clock(gpu, &before) &&
                fr_gas_gpu_audit(gpu, &audit, err, sizeof(err)) &&
                fr_gas_gpu_clock(gpu, &after));
    ASSERT_EQ(before.physical_steps, after.physical_steps);
    ASSERT_EQ(before.advanced_seconds, after.advanced_seconds);
    ASSERT_EQ(audit.metrics[FR_GAS_AUDIT_INVALID], 0.f);
    double authoritative = 0.;
    for (uint32_t tile = 0u; tile < tiles; ++tile) {
        ASSERT_TRUE(fr_gas_gpu_read_chunk(gpu, tile, gas, DC_CHUNK_CELLS, err, sizeof(err)));
        for (uint32_t cell = 0u; cell < DC_CHUNK_CELLS; ++cell)
            authoritative += gas[cell].species[FR_GAS_EXHAUST];
    }
    double measured = audit.metrics[FR_GAS_AUDIT_SPECIES + FR_GAS_EXHAUST];
    double error_fp = (measured - authoritative) * DC_FLUID_FULL;
    printf("Cold audit exhaust %.12g authoritative %.12g error %+.6f Q16\n",
           measured, authoritative, error_fp);
    ASSERT_TRUE(fabs(error_fp) <= 4.);
    ASSERT_EQ(audit.species[FR_GAS_EXHAUST], authoritative);
    free(volume);
    free(gas);
    free(chunk);
    dc_gpu_destroy(gpu);
    PASS();
}

#endif
