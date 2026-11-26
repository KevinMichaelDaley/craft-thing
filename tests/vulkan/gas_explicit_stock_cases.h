#ifndef GAS_EXPLICIT_STOCK_CASES_H
#define GAS_EXPLICIT_STOCK_CASES_H

static void check_explicit_line_stock(bool cached_rates) {
    const float seconds = 1.f / 64.f;
    const double density = 2., temperature = 300., velocity = 90.;
    const double exhaust[] = {1.03125, .123456};
    const double soot[] = {.0703125, .0000637};
    const uint32_t receiver = 1u;
    const fr_gas_model_t model = {
        .gas_constant = {.25, .25, .25, .25, .25},
        .heat_capacity_cv = {.75, .75, .75, .75, .75},
        .kinetic_scale = 1. / 512.,
    };
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, DC_CHUNK_SIDE, DC_CHUNK_SIDE,
        "build/shaders/pattern.comp.spv", err, sizeof(err)));
    if (!fr_gas_acoustic_supports_large_transport(gpu, FR_GAS_PRESSURE_PRACTICAL)) {
        printf("SKIP explicit large-step transport unavailable on this Vulkan device\n");
        ++g_skip;
        dc_gpu_destroy(gpu);
        return;
    }

    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_gas_state_t *stock = calloc(DC_CHUNK_CELLS, sizeof(*stock));
    double *volume = calloc(DC_CHUNK_CELLS, sizeof(*volume));
    ASSERT_TRUE(chunk && stock && volume);
    /* Each pair shares one horizontal face. The original gas volume stays fixed. */
    for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y) {
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
            uint32_t cell = y * DC_CHUNK_SIDE + x;
            uint32_t side = x % 4u;
            if (y % 2u || side >= 2u) {
                chunk->cells[cell].material = DC_MATERIAL_STONE;
                continue;
            }
            volume[cell] = 1.;
            stock[cell].species[FR_GAS_EXHAUST] = exhaust[side];
            stock[cell].species[FR_GAS_SOOT] = soot[side];
            stock[cell].species[FR_GAS_NITROGEN] = density - exhaust[side] - soot[side];
            stock[cell].momentum[0] = density * velocity;
            stock[cell].energy = density * (.75 * temperature + .5 * model.kinetic_scale * velocity * velocity);
        }
    }
    fr_gas_gpu_audit_t before, after;
    ASSERT_TRUE(dc_gpu_upload_chunk_exact(gpu, 0, chunk, false, err, sizeof(err)) &&
        dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)) &&
        fr_gas_gpu_enable(gpu, &model, (float)density, (float)temperature, err, sizeof(err)) &&
        fr_gas_gpu_set_pressure_policy(gpu, FR_GAS_PRESSURE_PRACTICAL, err, sizeof(err)) &&
        fr_gas_gpu_seed_chunk(gpu, 0, stock, volume, DC_CHUNK_CELLS, err, sizeof(err)) &&
        fr_gas_gpu_refresh_geometry(gpu, err, sizeof(err)) &&
        fr_gas_gpu_audit(gpu, &before, err, sizeof(err)) &&
        fr_gas_gpu_step(gpu, seconds, err, sizeof(err)) &&
        fr_gas_gpu_audit(gpu, &after, err, sizeof(err)));
    bool used_cached_rates = false;
    ASSERT_TRUE(gpu->gas->acoustic.large_transport &&
        fr_gas_gpu_local_stage_metadata(gpu, &used_cached_rates) &&
        fr_gas_gpu_read_chunk(gpu, 0, stock, DC_CHUNK_CELLS, err, sizeof(err)));
    double received = stock[receiver].species[FR_GAS_EXHAUST];
    double exhaust_drift = (after.species[FR_GAS_EXHAUST] - before.species[FR_GAS_EXHAUST]) * DC_FLUID_FULL;
    double soot_drift = (after.species[FR_GAS_SOOT] - before.species[FR_GAS_SOOT]) * DC_FLUID_FULL;
    dc_gpu_destroy(gpu);
    free(chunk);
    free(stock);
    free(volume);
    printf("Explicit pair stock with cached rates %u: exhaust %+.9f Q16, soot %+.9f Q16, receiver %.9f\n",
        used_cached_rates, exhaust_drift, soot_drift, received);
    ASSERT_EQ(used_cached_rates, cached_rates);
    ASSERT_EQ(before.metrics[FR_GAS_AUDIT_INVALID], 0.f);
    ASSERT_EQ(after.metrics[FR_GAS_AUDIT_INVALID], 0.f);
    ASSERT_TRUE(received > exhaust[receiver] + .001);
    ASSERT_TRUE(fabs(exhaust_drift) <= .001 && fabs(soot_drift) <= .001);
    PASS();
}

static void test_explicit_line_conserves_exhaust_and_soot_when_stocks_round(void) {
    char err[256] = {0};
    const char *const options[] = {"DC_GAS_FUSED_AXIS", "DC_GAS_FUSED_METADATA", "DC_GAS_HYBRID_LINES",
        "DC_GAS_FUSED_HYBRID", "DC_GAS_FIXED_AXIS", "DC_GAS_WIDE_SCAN", "DC_GAS_LINE_GROUP",
        "DC_GAS_EXPLICIT_RATE_CACHE"};
    char *previous[sizeof(options) / sizeof(options[0])] = {0};
    size_t count = sizeof(previous) / sizeof(previous[0]);
    for (size_t i = 0; i < count; ++i) {
        const char *old = getenv(options[i]);
        previous[i] = old ? SDL_strdup(old) : NULL;
        ASSERT_TRUE(!old || previous[i]);
        ASSERT_EQ(SDL_setenv(options[i], "1", 1), 0);
    }
    ASSERT_EQ(SDL_setenv("DC_GAS_EXPLICIT_RATE_CACHE", "0", 1), 0);
    check_explicit_line_stock(false);
    ASSERT_EQ(SDL_setenv("DC_GAS_EXPLICIT_RATE_CACHE", "1", 1), 0);
    check_explicit_line_stock(true);
    for (size_t i = 0; i < count; ++i) {
        int restored = previous[i] ? SDL_setenv(options[i], previous[i], 1) : unsetenv(options[i]);
        SDL_free(previous[i]);
        ASSERT_EQ(restored, 0);
    }
}

#endif
