#ifndef FERRUM_TEST_RIGID_COATING_CONTACT_CASES_H
#define FERRUM_TEST_RIGID_COATING_CONTACT_CASES_H

static double contact_film_heat(const uint32_t stock[3], double temperature) {
    double oil = fr_reactive_profile(DC_MATERIAL_OIL)->heat_capacity;
    double jelly = fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY)->heat_capacity;
    return (stock[0] * oil + (stock[1] + stock[2]) * jelly) * temperature / DC_FLUID_FULL;
}

static void check_rigid_coating_contacts(uint32_t count, const int destinations[3]) {
    enum {x = 20, y = 20, first = y * DC_CHUNK_SIDE + x};
    const uint32_t object_cells = DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS;
    const uint32_t stock[3][3] = {{32000u, 16000u, 12000u}, {8000u, 4000u, 8000u},
                                  {24000u, 12000u, 20000u}};
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    uint32_t *objects = calloc(object_cells, sizeof(*objects));
    ASSERT_TRUE(chunk && objects && context(&g, err, sizeof(err)));
    for (uint32_t i = 0; i < count; i++) {
        chunk->cells[first + i].material = DC_MATERIAL_WOOD;
        objects[first + i] = 1u + i;
    }
    ASSERT_TRUE(dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)));
    fr_reactive_state_t before[3] = {0};
    uint32_t expected[3][3] = {{0}};
    double expected_heat[3] = {0.};
    for (uint32_t i = 0; i < count; i++) {
        float temperature = 500.f + 30.f * i;
        ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 39322u, 0u, temperature,
                                              600300u + i, &before[i], err, sizeof(err)));
        if (destinations[i] >= 0) {
            before[i].coating_oil_fuel_fp = stock[i][0];
            before[i].coating_jelly_fuel_fp = stock[i][1];
            before[i].coating_oxidizer_fp = stock[i][2];
            before[i].coating_threshold = .01f;
            before[i].coating_integral = .02f;
            before[i].flags |= FR_REACTIVE_FLAMING | FR_REACTIVE_COATING_FLAMING;
            memcpy(expected[i], stock[i], sizeof(expected[i]));
        }
        before[i].thermal_energy = (float)fr_reactive_state_energy_at_temperature(&before[i], temperature);
        expected_heat[i] = fr_reactive_state_energy(&before[i]);
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(g, 0, first + i, &before[i], err, sizeof(err)));
    }
    ASSERT_TRUE(fr_gpu_admit_rigid_objects(g, 600300u, objects, object_cells, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_reserve_import(g, 70u, err, sizeof(err)));
    for (uint32_t i = 0; i < count; i++) {
        if (destinations[i] < 0) continue;
        uint32_t share[3];
        for (uint32_t species = 0; species < 3u; species++) {
            share[species] = stock[i][species] / 4u;
            expected[i][species] -= share[species];
            expected[destinations[i]][species] += share[species];
        }
        double heat = contact_film_heat(share, 500. + 30. * i);
        expected_heat[i] -= heat;
        expected_heat[destinations[i]] += heat;
    }
    fr_gas_model_t model = {.gas_constant = {.25984, .29680, .18892, .46152, 0},
        .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71}, .kinetic_scale = 1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g, err, sizeof(err)) &&
                fr_gas_gpu_enable(g, &model, .0012f, 293.15f, err, sizeof(err)) &&
                fr_combustion_gpu_enable(g, err, sizeof(err)) &&
                fr_combustion_gpu_step(g, 1e-7f, err, sizeof(err)));
    uint32_t seen = 0u;
    for (uint32_t probe = 0; probe < count; probe++) {
        fr_reactive_record_t result;
        bool found = false;
        ASSERT_TRUE(fr_gpu_reactive_read_body(g, 0x80000001u + probe, 0, 0, &result,
                                              &found, err, sizeof(err)) && found);
        uint32_t i = result.state.origin_id_low - before[0].origin_id_low;
        ASSERT_TRUE(i < count && fr_reactive_state_valid(&result.state, err, sizeof(err)));
        ASSERT_EQ(seen & (1u << i), 0u);
        seen |= 1u << i;
        printf("Rigid contact parcel %u film %u/%u/%u heat %.6f, expected %u/%u/%u %.6f\n",
               i, result.state.coating_oil_fuel_fp, result.state.coating_jelly_fuel_fp,
               result.state.coating_oxidizer_fp, fr_reactive_state_energy(&result.state),
               expected[i][0], expected[i][1], expected[i][2], expected_heat[i]);
        ASSERT_EQ(result.state.coating_oil_fuel_fp, expected[i][0]);
        ASSERT_EQ(result.state.coating_jelly_fuel_fp, expected[i][1]);
        ASSERT_EQ(result.state.coating_oxidizer_fp, expected[i][2]);
        ASSERT_EQ(result.state.fuel_mass_fp, before[i].fuel_mass_fp);
        ASSERT_TRUE(fabs(fr_reactive_state_energy(&result.state) - expected_heat[i]) < .005);
    }
    free(objects);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}

static void test_reciprocal_rigid_coating_contact_preserves_film_heat(void) {
    const int destinations[3] = {1, 0, -1};
    check_rigid_coating_contacts(2u, destinations);
}

static void test_competing_rigid_coating_contacts_publish_both_offers(void) {
    const int destinations[3] = {1, -1, 1};
    check_rigid_coating_contacts(3u, destinations);
}

static void run_rigid_coating_contact_cases(void) {
    RUN(test_reciprocal_rigid_coating_contact_preserves_film_heat);
    RUN(test_competing_rigid_coating_contacts_publish_both_offers);
}

#endif
