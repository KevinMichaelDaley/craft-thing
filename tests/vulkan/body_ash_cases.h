static void body_ash_case(bool blocked) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(uint32_t x=10;x<13;x++) chunk->cells[640+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t live,spent;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,300,4300,&live,err,sizeof(err))&&
        fr_reactive_state_default(DC_MATERIAL_WOOD,2000,0,300,4301,&spent,err,sizeof(err)));
    spent.inert_mass_fp=spent.fuel_mass_fp;spent.fuel_mass_fp=0;spent.flags=FR_REACTIVE_SPENT;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,650,&live,err,sizeof(err))&&fr_gpu_reactive_seed_cell(g,0,651,&spent,err,sizeof(err)));
    live.parcel_id_low=live.origin_id_low=4302;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,652,&live,err,sizeof(err))&&dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err)));
    if(blocked) {
        for(uint32_t x=10;x<13;x++) chunk->cells[640+x].material=DC_MATERIAL_AIR;
        chunk->cells[651].material=DC_MATERIAL_ASH;
        dc_chunk_particle_init(chunk->particles+651,(dc_chunk_coord_t){0,0},651,DC_MATERIAL_ASH);
        chunk->particles[651].mass_fp=100;chunk->particles[4747]=chunk->particles[651];
        chunk->particles[4747].id_lo++;chunk->particle_count=2;
        ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    }
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},.heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_step(g,.01f,err,sizeof(err))&&dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    if(blocked) {
        fr_reactive_record_t retained;bool found=false;
        ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,1,0,&retained,&found,err,sizeof(err))&&found);
        ASSERT_EQ(fr_reactive_state_mass_fp(&retained.state),2000);ASSERT_EQ(retained.state.origin_id_low,4301);
        ASSERT_EQ(retained.state.inert_mass_fp,spent.inert_mass_fp);
        ASSERT_TRUE(fabs(fr_reactive_state_energy(&retained.state)-fr_reactive_state_energy(&spent))<.01);
        ASSERT_EQ(chunk->particles[651].mass_fp,100);ASSERT_EQ(chunk->particles[4747].mass_fp,100);
        printf("Occupied ash slots retain original rigid residue and both existing particles\n");
        ASSERT_TRUE(present(g,err,sizeof(err)));free(chunk);dc_gpu_destroy(g);PASS();return;
    }
    uint32_t ash=UINT32_MAX;
    for(uint32_t i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) if(chunk->particles[i].mass_fp) {
        ASSERT_EQ(ash,UINT32_MAX);ash=i;ASSERT_EQ(chunk->particles[i].material,DC_MATERIAL_ASH);ASSERT_EQ(chunk->particles[i].mass_fp,2000);
    }
    ASSERT_TRUE(ash!=UINT32_MAX);
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,ash,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(record.state.material,DC_MATERIAL_ASH);ASSERT_EQ(record.state.origin_id_low,4301);
    ASSERT_EQ(fr_reactive_state_mass_fp(&record.state),2000);ASSERT_TRUE(fabs(fr_reactive_state_energy(&record.state)-fr_reactive_state_energy(&spent))<.01);
    dc_gpu_body_t left,right;
    ASSERT_TRUE(dc_gpu_read_body_id(g,0x80000001u,&left,err,sizeof(err))&&dc_gpu_read_body_id(g,0x80000002u,&right,err,sizeof(err)));
    ASSERT_TRUE(left.id!=right.id);
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/reactive_body_ash.dcs",err,sizeof(err)));
#ifdef FR_REACTIVE_PRESENTATION_TEST
    ASSERT_TRUE(dc_gpu_set_presentation_focus(g,(dc_presentation_focus_t){6,6,16,14}));
#endif
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);g=NULL;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&fr_gpu_load_structure(g,"build/reactive_body_ash.dcs",err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_record_t saved;
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,ash,&saved,&found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&record.state,&saved.state,sizeof(saved.state)),0);
    printf("Rigid ash2000Q16 retained identity/heat, two live fragments, checkpoint roundtrip\n");
    free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_spent_rigid_rest_becomes_loose_ash_and_splits_supported_geometry(void) {body_ash_case(false);}
static void test_full_particle_slots_retain_spent_rigid_inventory(void) {body_ash_case(true);}

static void mixed_concave_burn_case(bool blocked) {
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    dc_chunk_colors_t *colors = calloc(1, sizeof(*colors));
    ASSERT_TRUE(chunk && colors);
    for (uint32_t y = 10u; y <= 12u; y++)
        for (uint32_t x = 10u; x <= 14u; x++) {
            if (x != 10u && x != 14u && y != 12u) continue;
            uint32_t cell = y * 64u + x;
            chunk->cells[cell].material = x == 14u ? DC_MATERIAL_STONE :
                                                      DC_MATERIAL_WOOD;
            colors->terrain[cell][0] = (float)x / 64.f;
            colors->terrain[cell][1] = .25f;
            colors->terrain[cell][3] = 1.f;
        }
    ASSERT_TRUE(context(&g, err, sizeof(err)) &&
        dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)) &&
        dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)) &&
        fr_renderer_upload_chunk_colors(g, 0, colors, err, sizeof(err)));
    fr_reactive_state_t live, spent;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 39322u, 0u,
        1000.f, 5400u, &live, err, sizeof(err)) &&
        fr_reactive_state_default(DC_MATERIAL_WOOD, 2000u, 0u,
        300.f, 5401u, &spent, err, sizeof(err)));
    live.flags |= FR_REACTIVE_FLAMING;
    spent.inert_mass_fp = spent.fuel_mass_fp;
    spent.fuel_mass_fp = 0u;
    spent.flags = FR_REACTIVE_SPENT;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g, 0, 12u * 64u + 11u,
        &live, err, sizeof(err)) &&
        fr_gpu_reactive_seed_cell(g, 0, 12u * 64u + 12u,
        &spent, err, sizeof(err)) &&
        dc_gpu_admit_authored_rigid_cells(g, err, sizeof(err)));
    dc_gpu_body_pixel_t initial[9];
    uint32_t initial_count = 0u;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g, 0x80000001u, initial, 9u,
        &initial_count, err, sizeof(err)));
    ASSERT_EQ(initial_count, 9u);
    if (blocked) {
        for (uint32_t y = 10u; y <= 12u; y++)
            for (uint32_t x = 10u; x <= 14u; x++)
                chunk->cells[y * 64u + x].material = DC_MATERIAL_AIR;
        uint32_t ash_cell = 12u * 64u + 12u;
        chunk->cells[ash_cell].material = DC_MATERIAL_ASH;
        dc_chunk_particle_init(chunk->particles + ash_cell,
            (dc_chunk_coord_t){0, 0}, ash_cell, DC_MATERIAL_ASH);
        chunk->particles[ash_cell].mass_fp = 100u;
        chunk->particles[ash_cell + 4096u] = chunk->particles[ash_cell];
        chunk->particles[ash_cell + 4096u].id_lo++;
        chunk->particle_count = 2u;
        ASSERT_TRUE(dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)));
    }
    fr_gas_model_t model = {.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},
        .kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g, err, sizeof(err)) &&
        fr_gas_gpu_enable(g, &model, .0012f, 293.15f, err, sizeof(err)) &&
        fr_combustion_gpu_enable(g, err, sizeof(err)) &&
        fr_combustion_gpu_step(g, .01f, err, sizeof(err)));
    if (blocked) {
        dc_gpu_body_pixel_t unchanged[9];
        uint32_t unchanged_count = 0u;
        fr_reactive_record_t retained;
        bool found = false;
        ASSERT_TRUE(dc_gpu_read_pixel_shape(g, 0x80000001u, unchanged,
            9u, &unchanged_count, err, sizeof(err)) &&
            fr_gpu_reactive_read_body(g, 0x80000001u, 2u, 2u,
                &retained, &found, err, sizeof(err)) && found);
        ASSERT_EQ(unchanged_count, 9u);
        ASSERT_EQ(retained.state.origin_id_low, 5401u);
        ASSERT_EQ(retained.state.inert_mass_fp, spent.inert_mass_fp);
        ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
        ASSERT_EQ(chunk->particles[12u * 64u + 12u].mass_fp, 100u);
        ASSERT_EQ(chunk->particles[12u * 64u + 12u + 4096u].mass_fp,
                  100u);
        chunk->cells[12u * 64u + 12u].material = DC_MATERIAL_AIR;
        memset(chunk->particles, 0, sizeof(chunk->particles));
        chunk->particle_count = 0u;
        ASSERT_TRUE(dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)) &&
            fr_combustion_gpu_step(g, .01f, err, sizeof(err)));
    }
    bool seen[15] = {0};
    uint32_t pieces = 0u, live_parcels = 0u, spent_parcels = 0u;
    bool live_burned = false;
    double body_mass = 0.;
    for (uint32_t body_index = 0u; body_index < 2u; body_index++) {
        uint32_t id = 0x80000001u + body_index;
        dc_gpu_world_body_t body;
        dc_gpu_body_motion_t motion;
        dc_gpu_body_pixel_t pixels[9];
        uint32_t count = 0u;
        ASSERT_TRUE(dc_gpu_read_world_body(g, id, &body, err, sizeof(err)) &&
            dc_gpu_read_body_motion(g, id, &motion, err, sizeof(err)) &&
            dc_gpu_read_pixel_shape(g, id, pixels, 9u, &count,
                                    err, sizeof(err)));
        const dc_gpu_body_record_t *physical = NULL;
        for (uint32_t slot = 0u; slot < dc_gpu_body_capacity(g); slot++) {
            const dc_gpu_body_record_t *candidate = dc_gpu_body_record(g, slot);
            if (candidate->body.active && candidate->body.id == id) {
                physical = candidate;
                break;
            }
        }
        ASSERT_TRUE(physical);
        const uint32_t *shape = (const uint32_t *)g->body_mapped +
                                physical->compound_reserved;
        body_mass += motion.mass;
        for (uint32_t i = 0u; i < count; i++) {
            uint32_t x = (uint32_t)(body.body.x_fp >> 16) + pixels[i].x;
            uint32_t y = (uint32_t)(body.body.y_fp >> 16) + pixels[i].y;
            ASSERT_TRUE(x >= 10u && x <= 14u && y >= 10u && y <= 12u);
            uint32_t cell = (y - 10u) * 5u + x - 10u;
            ASSERT_TRUE(!seen[cell]);
            seen[cell] = true;
            ASSERT_TRUE(x != 12u || y != 12u);
            ASSERT_EQ(pixels[i].material, x == 14u ? DC_GPU_BODY_STONE :
                                                     DC_GPU_BODY_WOOD);
            uint32_t tile = (pixels[i].y / 16u) * shape[2] +
                            pixels[i].x / 16u;
            uint32_t local = (pixels[i].y % 16u) * 16u + pixels[i].x % 16u;
            uint32_t word = physical->compound_reserved + shape[8u + tile] +
                            1u + local / 8u;
            const float *rgba = dc_gpu_shape_colors(g, word) +
                                (local % 8u) * 4u;
            if (x == 11u && y == 12u) {
                ASSERT_TRUE(rgba[1] > 0.f);
                ASSERT_TRUE(fabsf(rgba[0] / rgba[1] - 11.f / 16.f) < .001f);
            } else {
                ASSERT_TRUE(fabsf(rgba[0] - (float)x / 64.f) < 1e-6f);
                ASSERT_EQ(rgba[1], .25f);
            }
            ASSERT_EQ(rgba[3], 1.f);
            fr_reactive_record_t record;
            bool found = false;
            ASSERT_TRUE(fr_gpu_reactive_read_body(g, id, pixels[i].x,
                pixels[i].y, &record, &found, err, sizeof(err)));
            if (found && record.state.origin_id_low == 5400u) {
                if (record.state.fuel_mass_fp < live.fuel_mass_fp)
                    live_burned = true;
                live_parcels++;
            }
            if (found && record.state.origin_id_low == 5401u) spent_parcels++;
            pieces++;
        }
    }
    ASSERT_EQ(pieces, 8u);
    ASSERT_EQ(live_parcels, 1u);
    ASSERT_TRUE(live_burned);
    ASSERT_EQ(spent_parcels, 0u);
    ASSERT_TRUE(fabs(body_mass - 11.1) < .01);
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    unsigned occupied = 0u;
    ASSERT_TRUE(dc_gpu_read_occupancy(g, 12u, 12u, &occupied, NULL,
        err, sizeof(err)));
    ASSERT_EQ(occupied, 0u);
    ASSERT_TRUE(dc_gpu_read_occupancy(g, 11u, 12u, &occupied, NULL,
        err, sizeof(err)));
    ASSERT_TRUE(occupied == 0x80000001u || occupied == 0x80000002u);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    uint32_t ash_count = 0u;
    for (uint32_t i = 0u; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
        if (chunk->particles[i].material != DC_MATERIAL_ASH ||
            chunk->particles[i].mass_fp == 0u) continue;
        ASSERT_EQ(chunk->particles[i].mass_fp, 2000u);
        fr_reactive_record_t ash;
        bool found = false;
        ASSERT_TRUE(fr_gpu_reactive_read_particle(g, i, &ash, &found,
            err, sizeof(err)) && found);
        ASSERT_EQ(ash.state.origin_id_low, 5401u);
        ash_count++;
    }
    ASSERT_EQ(ash_count, 1u);
    free(colors);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}
static void test_mixed_concave_burn_preserves_color_state_and_loose_ash(void) {
    mixed_concave_burn_case(false);
}
static void test_mixed_concave_burn_retries_geometry_and_ash_together(void) {
    mixed_concave_burn_case(true);
}

static void test_burning_terrain_support_extracts_remnant_without_ash_support(void) {
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (uint32_t x = 0u; x < 64u; x++)
        chunk->cells[32u * 64u + x] = (dc_cell_t){
            .material = DC_MATERIAL_STONE, .flags = DC_CELL_LOADED_TERRAIN};
    for (uint32_t y = 20u; y < 32u; y++)
        chunk->cells[y * 64u + 20u] = (dc_cell_t){
            .material = DC_MATERIAL_WOOD, .flags = DC_CELL_LOADED_TERRAIN};
    for (uint32_t x = 21u; x <= 25u; x++)
        chunk->cells[20u * 64u + x] = (dc_cell_t){
            .material = DC_MATERIAL_WOOD, .flags = DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g, err, sizeof(err)) &&
        dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)) &&
        dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)) &&
        dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)) &&
        dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(!dc_gpu_read_body_id(g, 0x80000001u, &body,
                                      err, sizeof(err)));
    fr_reactive_state_t spent;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 2000u, 0u,
        300.f, 5500u, &spent, err, sizeof(err)));
    spent.inert_mass_fp = spent.fuel_mass_fp;
    spent.fuel_mass_fp = 0u;
    spent.flags = FR_REACTIVE_SPENT;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g, 0, 26u * 64u + 20u,
        &spent, err, sizeof(err)));
    fr_gas_model_t model = {.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},
        .kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g, err, sizeof(err)) &&
        fr_gas_gpu_enable(g, &model, .0012f, 293.15f, err, sizeof(err)) &&
        fr_combustion_gpu_enable(g, err, sizeof(err)) &&
        dc_gpu_tick_step(g, err, sizeof(err)) &&
        dc_gpu_tick_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(g, 0x80000001u, &body,
                                     err, sizeof(err)));
    dc_gpu_body_pixel_t pixels[17];
    uint32_t count = 0u;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g, body.id, pixels, 17u,
        &count, err, sizeof(err)));
    ASSERT_EQ(count, 11u);
    for (uint32_t i = 0u; i < count; i++)
        ASSERT_EQ(pixels[i].material, DC_GPU_BODY_WOOD);
    dc_gpu_world_body_t released, fallen;
    ASSERT_TRUE(dc_gpu_read_world_body(g, body.id, &released,
                                       err, sizeof(err)));
    for (uint32_t tick = 0u; tick < 20u; tick++)
        ASSERT_TRUE(dc_gpu_tick_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g, body.id, &fallen,
                                       err, sizeof(err)));
    ASSERT_TRUE(fallen.body.y_fp > released.body.y_fp);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    uint32_t ash_count = 0u;
    for (uint32_t i = 0u; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
        if (chunk->particles[i].material != DC_MATERIAL_ASH ||
            chunk->particles[i].mass_fp == 0u) continue;
        ASSERT_EQ(chunk->particles[i].mass_fp, 2000u);
        ash_count++;
    }
    ASSERT_EQ(ash_count, 1u);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}
