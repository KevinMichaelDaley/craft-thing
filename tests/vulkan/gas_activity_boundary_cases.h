#ifndef GAS_ACTIVITY_BOUNDARY_CASES_H
#define GAS_ACTIVITY_BOUNDARY_CASES_H

static void check_practical_exhaust_at_dormant_donor(bool large) {
    enum { PATCH_X = 24, PATCH_Y = 32, STOCK_EDGE = 40, ACTIVITY_FIELD = 9 };
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    ASSERT_TRUE(dc_gpu_create(&g, DC_CHUNK_SIDE, DC_CHUNK_SIDE,
        "build/shaders/pattern.comp.spv", err, sizeof(err)));
    if (large && !fr_gas_acoustic_supports_large_transport(g, FR_GAS_PRESSURE_PRACTICAL)) {
        printf("SKIP practical large-step transport unavailable on this Vulkan device\n");
        ++g_skip;
        dc_gpu_destroy(g);
        return;
    }

    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_gas_state_t *states = calloc(DC_CHUNK_CELLS, sizeof(*states));
    double *volume = calloc(DC_CHUNK_CELLS, sizeof(*volume));
    ASSERT_TRUE(chunk && states && volume);
    const fr_gas_model_t model = {
        .gas_constant = {.25, .25, .25, .25, 0},
        .heat_capacity_cv = {.75, .75, .75, .75, .75},
        .kinetic_scale = 1. / 512.,
    };
    const double density = .0012, temperature = 300.;
    for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y) {
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
            uint32_t i = y * DC_CHUNK_SIDE + x;
            double exhaust = x >= STOCK_EDGE ? .0001 : 0.;
            double velocity = x == PATCH_X && y == PATCH_Y ? (large ? 120. : .25) : 0.;
            volume[i] = 1.;
            states[i].species[FR_GAS_OXYGEN] = .21 * density;
            states[i].species[FR_GAS_NITROGEN] = .79 * density - exhaust;
            states[i].species[FR_GAS_EXHAUST] = exhaust;
            states[i].momentum[0] = density * velocity;
            states[i].energy = density * (.75 * temperature + .5 * model.kinetic_scale * velocity * velocity);
        }
    }
    fr_gas_gpu_audit_t before, after;
    ASSERT_TRUE(dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)) &&
        dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)) &&
        fr_gas_gpu_enable(g, &model, (float)density, (float)temperature, err, sizeof(err)) &&
        fr_gas_gpu_set_pressure_policy(g, FR_GAS_PRESSURE_PRACTICAL, err, sizeof(err)) &&
        fr_gas_gpu_seed_chunk(g, 0, states, volume, DC_CHUNK_CELLS, err, sizeof(err)) &&
        fr_gas_gpu_audit(g, &before, err, sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_step(g, 1.f / 60.f, err, sizeof(err)) &&
        fr_gas_gpu_audit(g, &after, err, sizeof(err)));
    const uint32_t *activity = g->gas->acoustic.fields[ACTIVITY_FIELD].mapped;
    uint32_t tiles_across = DC_CHUNK_SIDE / 8u;
    uint32_t receiver = tiles_across * tiles_across + (PATCH_Y / 8u) * tiles_across + (STOCK_EDGE - 1u) / 8u;
    bool receiver_advected = (activity[receiver] & 1u) != 0u;
    bool donor_advected = (activity[receiver + 1u] & 1u) != 0u;
    bool used_large = g->gas->acoustic.large_transport;
    double drift = (after.species[FR_GAS_EXHAUST] - before.species[FR_GAS_EXHAUST]) * DC_FLUID_FULL;
    printf("Closed sparse transport large %u receiver %u donor %u exhaust drift %+.9f Q16\n",
        used_large, receiver_advected, donor_advected, drift);
    dc_gpu_destroy(g);
    free(chunk);
    free(states);
    free(volume);
    ASSERT_EQ(used_large, large);
    ASSERT_TRUE(receiver_advected && !donor_advected);
    ASSERT_EQ(after.metrics[FR_GAS_AUDIT_INVALID], 0.f);
    ASSERT_TRUE(fabs(drift) <= .01);
    PASS();
}

static void check_sparse_practical_boundary(bool large) {
    char err[64] = {0};
    const char *options[] = {"DC_GAS_FUSED_AXIS", "DC_GAS_FUSED_METADATA", "DC_GAS_SPARSE_LARGE"};
    char *previous[sizeof(options) / sizeof(options[0])] = {0};
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        const char *old = SDL_getenv(options[i]);
        previous[i] = old ? SDL_strdup(old) : NULL;
        ASSERT_TRUE(!old || previous[i]);
        ASSERT_EQ(SDL_setenv(options[i], "1", 1), 0);
    }
    check_practical_exhaust_at_dormant_donor(large);
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        int restored = previous[i] ? SDL_setenv(options[i], previous[i], 1) : unsetenv(options[i]);
        SDL_free(previous[i]);
        ASSERT_EQ(restored, 0);
    }
}

static void test_practical_small_step_preserves_exhaust_at_dormant_donor(void) {
    check_sparse_practical_boundary(false);
}

static void test_practical_large_step_preserves_exhaust_at_dormant_donor(void) {
    check_sparse_practical_boundary(true);
}
#endif
