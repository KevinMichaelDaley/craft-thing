static double submerged_packet_energy(const float packet[16]) {
    return (double)packet[5] + packet[11] + packet[14];
}

static void test_coating_products_survive_sealing_reload_and_pressure_release(void) {
    enum { X = 20, Y = 20, SOURCE = Y * DC_CHUNK_SIDE + X, HOST = SOURCE - 1,
           RECEIVER = SOURCE + 1 };
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_gas_checkpoint_chunk_t *pending = malloc(sizeof(*pending));
    fr_gas_checkpoint_chunk_t *snapshot = malloc(sizeof(*snapshot));
    float *water_heat = calloc(DC_CHUNK_CELLS, sizeof(*water_heat));
    ASSERT_TRUE(chunk && pending && snapshot && water_heat);
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        chunk->cells[i].material = DC_MATERIAL_STONE;
    chunk->cells[HOST].flags = DC_CELL_LOADED_TERRAIN;
    chunk->cells[SOURCE] = (dc_cell_t){.material = DC_MATERIAL_WATER, .fluid_mass = DC_FLUID_FULL};
    chunk->cells[RECEIVER] = (dc_cell_t){.material = DC_MATERIAL_WATER, .fluid_mass = DC_FLUID_FULL / 2u};

    const fr_gas_model_t model = {
        .gas_constant = {.25984, .29680, .18892, .46152, 0},
        .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71}, .kinetic_scale = 1. / 512.
    };
    ASSERT_TRUE(context(&g, err, sizeof(err)) && dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)));
    fr_reactive_state_t film;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE, DC_FLUID_FULL, DC_FLUID_FULL / 4u,
                                           800.f, 120101u, &film, err, sizeof(err)));
    film.coating_oil_fuel_fp = film.coating_jelly_fuel_fp = film.coating_oxidizer_fp = DC_FLUID_FULL / 2u;
    film.flags |= FR_REACTIVE_COATING_FLAMING;
    film.coating_threshold = .01f;
    film.coating_integral = .02f;
    film.thermal_energy = (float)fr_reactive_state_energy_at_temperature(&film, 800.f);
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g, 0, HOST, &film, err, sizeof(err)) &&
                fr_thermal_gpu_enable(g, err, sizeof(err)) &&
                fr_gas_gpu_enable(g, &model, .0012f, 293.15f, err, sizeof(err)) &&
                fr_gas_gpu_set_pressure_policy(g, FR_GAS_PRESSURE_PRACTICAL, err, sizeof(err)) &&
                fr_combustion_gpu_enable(g, err, sizeof(err)) &&
                fr_gas_gpu_gate(g, X, Y, 1, 1, 0, true, err, sizeof(err)) &&
                fr_gas_gpu_export_checkpoint(g, 0, snapshot, err, sizeof(err)));
    ASSERT_EQ(snapshot->cells[SOURCE][12], 0.f);
    ASSERT_EQ(snapshot->cells[SOURCE][FR_GAS_EXHAUST], 0.f);
    const double energy_before = fr_reactive_state_energy(&film) +
        submerged_packet_energy(snapshot->cells[SOURCE]);
    ASSERT_TRUE(fr_combustion_gpu_step(g, .1f, err, sizeof(err)) &&
                fr_gas_gpu_export_checkpoint(g, 0, pending, err, sizeof(err)));
    fr_reactive_record_t reacted;
    bool found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g, 0, HOST, &reacted, &found, err, sizeof(err)) && found);
    const uint32_t oil = film.coating_oil_fuel_fp - reacted.state.coating_oil_fuel_fp;
    const uint32_t jelly = film.coating_jelly_fuel_fp - reacted.state.coating_jelly_fuel_fp;
    const uint32_t oxidizer = film.coating_oxidizer_fp - reacted.state.coating_oxidizer_fp;
    const uint32_t ash = reacted.state.inert_mass_fp - film.inert_mass_fp;
    const double exhaust = pending->cells[SOURCE][FR_GAS_EXHAUST];
    const double soot = pending->cells[SOURCE][FR_GAS_SOOT];
    const double chemical = (oil * (double)fr_reactive_profile(DC_MATERIAL_OIL)->primary_heat +
                             jelly * (double)fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY)->primary_heat) /
                            DC_FLUID_FULL - soot * FR_COMBUSTION_AIRBORNE_ENERGY;
    const double reaction_energy = fr_reactive_state_energy(&reacted.state) +
                                   submerged_packet_energy(pending->cells[SOURCE]) - energy_before;
    printf("Sealed coating spent %u fuel and %u oxidizer, pending %.6f Q16 gas, energy %.6f of %.6f\n",
           oil + jelly, oxidizer, (exhaust + soot) * DC_FLUID_FULL, reaction_energy, chemical);
    ASSERT_TRUE(oil > 0u && jelly > 0u && oxidizer > 0u && exhaust > 0. && soot > 0.);
    ASSERT_EQ(pending->cells[SOURCE][12], 0.f);
    ASSERT_TRUE(fabs((exhaust + soot) * DC_FLUID_FULL - (oil + jelly + oxidizer - ash)) < .001);
    ASSERT_EQ(reacted.state.bound_water_fp, film.bound_water_fp);
    ASSERT_TRUE(fabs(reaction_energy - chemical) < .001);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->cells[SOURCE].fluid_mass, DC_FLUID_FULL);
    ASSERT_EQ(chunk->cells[RECEIVER].fluid_mass, DC_FLUID_FULL / 2u);
    dc_gpu_destroy(g);
    g = NULL;

    ASSERT_TRUE(context(&g, err, sizeof(err)) && dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)) && fr_thermal_gpu_enable(g, err, sizeof(err)) &&
                fr_gas_gpu_enable(g, &model, .0012f, 293.15f, err, sizeof(err)) &&
                fr_gas_gpu_set_pressure_policy(g, FR_GAS_PRESSURE_PRACTICAL, err, sizeof(err)) &&
                fr_gas_gpu_import_checkpoint(g, 0, pending, err, sizeof(err)) &&
                fr_gas_gpu_export_checkpoint(g, 0, snapshot, err, sizeof(err)));
    ASSERT_EQ(memcmp(snapshot->cells, pending->cells, sizeof(snapshot->cells)), 0);
    ASSERT_EQ(memcmp(snapshot->gates, pending->gates, sizeof(snapshot->gates)), 0);
    ASSERT_TRUE(fr_gas_gpu_step(g, .02f, err, sizeof(err)) &&
                fr_gas_gpu_step(g, .02f, err, sizeof(err)) &&
                fr_gas_gpu_export_checkpoint(g, 0, snapshot, err, sizeof(err)));
    ASSERT_EQ(snapshot->cells[SOURCE][12], 0.f);
    ASSERT_EQ(snapshot->cells[SOURCE][FR_GAS_EXHAUST], exhaust);
    ASSERT_EQ(snapshot->cells[SOURCE][FR_GAS_SOOT], soot);
    ASSERT_TRUE(fabs(submerged_packet_energy(snapshot->cells[SOURCE]) -
                     submerged_packet_energy(pending->cells[SOURCE])) < 1e-8);
    ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(g, 0, water_heat, DC_CHUNK_CELLS, err, sizeof(err)));
    const double sealed_energy = submerged_packet_energy(snapshot->cells[SOURCE]) +
                                 submerged_packet_energy(snapshot->cells[RECEIVER]) +
                                 water_heat[SOURCE] + water_heat[RECEIVER];

    ASSERT_TRUE(fr_gas_gpu_gate(g, X, Y, 1, 1, 0, false, err, sizeof(err)) &&
                fr_gas_gpu_step(g, .02f, err, sizeof(err)) &&
                dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)) &&
                fr_gas_gpu_export_checkpoint(g, 0, snapshot, err, sizeof(err)) &&
                fr_thermal_gpu_read_water_chunk(g, 0, water_heat, DC_CHUNK_CELLS, err, sizeof(err)));
    ASSERT_TRUE(snapshot->cells[SOURCE][12] > 0.f && snapshot->cells[SOURCE][12] <= 1.f);
    ASSERT_TRUE(chunk->cells[SOURCE].fluid_mass < DC_FLUID_FULL &&
                chunk->cells[RECEIVER].fluid_mass > DC_FLUID_FULL / 2u);
    ASSERT_EQ(chunk->cells[SOURCE].fluid_mass + chunk->cells[RECEIVER].fluid_mass,
              DC_FLUID_FULL + DC_FLUID_FULL / 2u);
    ASSERT_TRUE(fabs((double)snapshot->cells[SOURCE][FR_GAS_EXHAUST] +
                     snapshot->cells[RECEIVER][FR_GAS_EXHAUST] - exhaust) < 1e-7);
    ASSERT_TRUE(fabs((double)snapshot->cells[SOURCE][FR_GAS_SOOT] +
                     snapshot->cells[RECEIVER][FR_GAS_SOOT] - soot) < 1e-7);
    const double released_energy = submerged_packet_energy(snapshot->cells[SOURCE]) +
                                   submerged_packet_energy(snapshot->cells[RECEIVER]) +
                                   water_heat[SOURCE] + water_heat[RECEIVER];
    printf("Coating pocket displaced %u Q16 water, energy drift %.9g\n",
           DC_FLUID_FULL - chunk->cells[SOURCE].fluid_mass, released_energy - sealed_energy);
    ASSERT_TRUE(fabs(released_energy - sealed_energy) < .02);
    free(water_heat);
    free(snapshot);
    free(pending);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}
