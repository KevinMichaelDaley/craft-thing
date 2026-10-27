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
