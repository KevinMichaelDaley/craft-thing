static void test_constructed_stone_owns_heat_without_changing_its_collider_or_support(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->cells[13*64+10]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_structural_gpu_graph_t graph={.node_count=2,.member_count=1,.anchor_count=1};
    graph.nodes[0]=(fr_structural_gpu_node_t){.id=1,.generation=1,.x_fp=10*65536,.y_fp=12*65536};
    graph.nodes[1]=graph.nodes[0];graph.nodes[1].id=2;graph.nodes[1].x_fp=22*65536;
    graph.members[0]=(fr_structural_gpu_member_t){1,1,1,2,FR_STRUCTURAL_STONE,2*65536,0,0};
    graph.anchors[0]=(fr_structural_gpu_anchor_t){.id=1,.generation=1,.node=1,.kind=FR_JOINT_MOMENT,
        .x=10,.y=13,.width=1,.height=1,.terrain_material=DC_MATERIAL_STONE,
        .normal_stiffness=1e8,.shear_stiffness=1e8,.rotational_stiffness=1e8,
        .bearing_capacity=1e7,.pullout_capacity=1e7,.shear_capacity=1e7,.moment_capacity=1e7,.friction=.5};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&graph,err,sizeof(err)));
    dc_gpu_body_motion_t before,after;ASSERT_TRUE(dc_gpu_read_body_motion(g,FR_STRUCTURAL_BODY_ID_BASE+1,&before,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err)));
    fr_reactive_record_t stone;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,FR_STRUCTURAL_BODY_ID_BASE+1,5,0,&stone,&found,err,sizeof(err))&&found);
    ASSERT_EQ(stone.state.material,DC_MATERIAL_STONE);
    ASSERT_TRUE(fr_thermal_gpu_probe(g,15,11,true)&&fr_thermal_gpu_step(g,.25f,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,FR_STRUCTURAL_BODY_ID_BASE+1,5,0,&stone,&found,err,sizeof(err))&&found);
    float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(&stone.state,&temperature,err,sizeof(err))&&temperature>FR_THERMAL_AMBIENT_K);
    ASSERT_TRUE(dc_gpu_read_body_motion(g,FR_STRUCTURAL_BODY_ID_BASE+1,&after,err,sizeof(err)));
    ASSERT_TRUE(after.mass==before.mass&&after.inertia==before.inertia&&after.angle==before.angle);
    uint32_t pixels=0;ASSERT_TRUE(!dc_gpu_read_pixel_shape(g,FR_STRUCTURAL_BODY_ID_BASE+1,NULL,0,&pixels,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/constructed_stone_heat.dcs",err,sizeof(err))&&present(g,err,sizeof(err)));
    fr_reactive_state_t saved=stone.state;
    ASSERT_TRUE(fr_gpu_load_structure(g,"build/constructed_stone_heat.dcs",err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,FR_STRUCTURAL_BODY_ID_BASE+1,5,0,&stone,&found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&saved,&stone.state,sizeof(saved)),0);
    ASSERT_TRUE(dc_gpu_paint_material(g,10,11,0,DC_MATERIAL_AIR,err,sizeof(err)));
    uint32_t parcels=0,retained=0;
    for(uint32_t i=0;i<dc_gpu_body_capacity(g);i++) {
        const dc_gpu_body_record_t *body=dc_gpu_body_record(g,i);
        if(!body->body.active) continue;
        for(uint32_t y=0;y<body->body.height;y++) for(uint32_t x=0;x<body->body.width;x++) {
            ASSERT_TRUE(fr_gpu_reactive_read_body(g,body->body.id,x,y,&stone,&found,err,sizeof(err)));
            if(!found) continue;
            parcels++;
            if(stone.state.parcel_id_low==saved.parcel_id_low&&stone.state.parcel_id_high==saved.parcel_id_high) {
                retained++;ASSERT_EQ(memcmp(&saved,&stone.state,sizeof(saved)),0);
            }
        }
    }
    ASSERT_EQ(parcels,23u);ASSERT_EQ(retained,1u);
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/constructed_stone_carved_heat.dcs",err,sizeof(err)));
    free(chunk);dc_gpu_destroy(g);PASS();
}

static void test_constructed_wood_burns_to_loose_ash_after_capacity_retry(void) {
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    chunk->cells[13 * 64 + 10] = (dc_cell_t){.material = DC_MATERIAL_STONE,
        .flags = DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g, err, sizeof(err)) &&
        dc_gpu_upload_chunk(g, 0, chunk, err, sizeof(err)) &&
        dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)));
    fr_structural_gpu_graph_t graph = {.node_count = 2, .member_count = 1,
        .anchor_count = 1};
    graph.nodes[0] = (fr_structural_gpu_node_t){.id = 1, .generation = 1,
        .x_fp = 10 * 65536, .y_fp = 12 * 65536};
    graph.nodes[1] = graph.nodes[0];
    graph.nodes[1].id = 2;
    graph.nodes[1].x_fp = 22 * 65536;
    graph.members[0] = (fr_structural_gpu_member_t){1, 1, 1, 2,
        FR_STRUCTURAL_WOOD, 2 * 65536, 0, 0};
    graph.anchors[0] = (fr_structural_gpu_anchor_t){.id = 1,
        .generation = 1, .node = 1, .kind = FR_JOINT_MOMENT,
        .x = 10, .y = 13, .width = 1, .height = 1,
        .terrain_material = DC_MATERIAL_STONE,
        .normal_stiffness = 1e8, .shear_stiffness = 1e8,
        .rotational_stiffness = 1e8, .bearing_capacity = 1e7,
        .pullout_capacity = 1e7, .shear_capacity = 1e7,
        .moment_capacity = 1e7, .friction = .5};
    ASSERT_TRUE(fr_gpu_place_structure(g, &graph, err, sizeof(err)) &&
        fr_thermal_gpu_enable(g, err, sizeof(err)) &&
        dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_reactive_record_t wood;
    bool found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
        5, 0, &wood, &found, err, sizeof(err)) && found);
    ASSERT_EQ(wood.owner.kind, FR_REACTIVE_OWNER_BODY);
    ASSERT_EQ(wood.state.material, DC_MATERIAL_WOOD);
    ASSERT_TRUE(wood.state.fuel_mass_fp > 0);
    dc_gpu_body_motion_t initial_motion, burning_motion;
    fr_structural_gpu_member_state_t sound_member, burned_member;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
        &initial_motion, err, sizeof(err)) &&
        fr_gpu_read_structure_member(g, 1, &sound_member));
    uint32_t *entries = g->reactive->arena.entries.mapped;
    uint32_t allocated = ((uint32_t *)g->reactive->arena.root.mapped)[12];
    uint32_t *target_state = NULL;
    for(uint32_t index = 1; index < allocated; index++) {
        uint32_t *entry = entries + (size_t)index * FR_ENTRY_WORDS;
        if(entry[FR_ENTRY_OWNER] != FR_REACTIVE_OWNER_BODY ||
           entry[FR_ENTRY_OWNER + 1] != FR_STRUCTURAL_BODY_ID_BASE + 1 ||
           entry[FR_ENTRY_OWNER + 3] != 5 || entry[FR_ENTRY_OWNER + 4] != 0)
            continue;
        target_state = entry + FR_ENTRY_STATE;
        break;
    }
    ASSERT_TRUE(target_state);
    fr_gas_model_t model = {.gas_constant = {.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv = {.65816,.74320,.65508,1.40248,.71},
        .kinetic_scale = 1./512.};
    ASSERT_TRUE(fr_gas_gpu_enable(g, &model, .0012f, 293.15f,
        err, sizeof(err)) && fr_combustion_gpu_enable(g, err, sizeof(err)));
    target_state[9] /= 2u;
    ASSERT_TRUE(fr_combustion_gpu_step(g, .01f, err, sizeof(err)) &&
        dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
        &burning_motion, err, sizeof(err)));
    ASSERT_TRUE(burning_motion.mass < initial_motion.mass - .1f);
    ASSERT_TRUE(burning_motion.center_x > initial_motion.center_x);
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)) &&
        fr_gpu_read_structure_member(g, 1, &burned_member));
    ASSERT_TRUE(burned_member.damage > sound_member.damage + .1f);
    memcpy(target_state, &wood.state, sizeof(wood.state));
    target_state[11] = target_state[9];
    target_state[9] = 0;
    target_state[1] = FR_REACTIVE_SPENT;
    ASSERT_TRUE(
        fr_combustion_gpu_step(g, .01f, err, sizeof(err)));
    fr_reactive_record_t after_ash;
    bool remains = false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
        5, 0, &after_ash, &remains, err, sizeof(err)) && remains);
    ASSERT_EQ(after_ash.state.inert_mass_fp, wood.state.fuel_mass_fp);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particles[12 * 64 + 15].mass_fp, 0u);
    ASSERT_TRUE(((uint32_t *)g->runtime_bodies->work.mapped)[13] <
                ((uint32_t *)g->runtime_bodies->mailbox.mapped)[9]);
    ASSERT_TRUE(fr_combustion_gpu_step(g, .01f, err, sizeof(err)));
    ASSERT_TRUE(((uint32_t *)g->runtime_bodies->work.mapped)[13] >= 256u);
    dc_gpu_body_pixel_t pixels[256];
    uint32_t count = 0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
        pixels, 256, &count, err, sizeof(err)));
    ASSERT_EQ(count, 23u);
    for(uint32_t i = 0; i < count; i++)
        ASSERT_TRUE(pixels[i].x != 5u || pixels[i].y != 0u);
    dc_gpu_body_motion_t remnant;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
        &remnant, err, sizeof(err)));
    ASSERT_TRUE(fabs((double)remnant.mass - 23. * DC_DENSITY_WOOD) < .001);
    ASSERT_TRUE(fr_combustion_gpu_step(g, .01f, err, sizeof(err)) &&
        dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
        &remnant, err, sizeof(err)));
    ASSERT_TRUE(fabs((double)remnant.mass - 23. * DC_DENSITY_WOOD) < .001);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    bool ash = false;
    for(uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++)
        if(chunk->particles[i].material == DC_MATERIAL_ASH &&
           chunk->particles[i].mass_fp == wood.state.fuel_mass_fp) ash = true;
    ASSERT_TRUE(ash);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}
