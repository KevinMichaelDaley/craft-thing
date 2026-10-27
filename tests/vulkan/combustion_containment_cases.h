static void test_stone_and_clearance_contain_while_a_fuel_bridge_spreads(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(uint32_t lane=0;lane<3;lane++) {
        uint32_t y=12+lane*16;
        for(uint32_t x=20;x<=22;x++) if(x!=21||lane!=0)
            chunk->cells[y*64+x]=(dc_cell_t){.material=(x==21&&lane==1)?DC_MATERIAL_STONE:DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
        for(uint32_t x=18;x<25;x++) chunk->cells[(y+1)*64+x]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) if(chunk->cells[i].material) {
        fr_reactive_state_t state;uint32_t x=i%64,material=chunk->cells[i].material;
        ASSERT_TRUE(fr_reactive_state_default(material,material==DC_MATERIAL_WOOD?39322:176947,0,
            material==DC_MATERIAL_WOOD&&x==20?1100:293.15,1000+i,&state,err,sizeof(err)));
        if(material==DC_MATERIAL_WOOD&&x==20) {state.flags=FR_REACTIVE_FLAMING;state.ignition_threshold=.5;state.ignition_integral=.6;}
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,i,&state,err,sizeof(err)));
    }
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_mark_bonfire_world(g));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_enable(g,err,sizeof(err)));
    for(uint32_t step=0;step<1000;step++) {
        ASSERT_TRUE(fr_thermal_gpu_step(g,.02f,err,sizeof(err))&&fr_gas_gpu_step(g,.02f,err,sizeof(err)));
        if(step%100==0) ASSERT_TRUE(present(g,err,sizeof(err)));
    }
    uint32_t fuel[3];
    for(uint32_t lane=0;lane<3;lane++) {
        fr_reactive_record_t target;bool found=false;uint32_t at=(12+lane*16)*64+22;
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&target,&found,err,sizeof(err))&&found);
        float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(&target.state,&temperature,err,sizeof(err)));
        fuel[lane]=target.state.fuel_mass_fp;printf("Containment lane%u target fuel%u temperature%.3fK\n",lane,fuel[lane],temperature);
        for(uint32_t x=20;x<22;x++) if(chunk->cells[(12+lane*16)*64+x].material==DC_MATERIAL_WOOD) {
            ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,(12+lane*16)*64+x,&target,&found,err,sizeof(err))&&found);
            ASSERT_TRUE(fr_reactive_state_temperature(&target.state,&temperature,err,sizeof(err)));
            printf("  wood x%u fuel%u temperature%.3fK\n",x,target.state.fuel_mass_fp,temperature);
        }
    }
    ASSERT_TRUE(fuel[2]<39322&&fuel[0]==39322&&fuel[1]==39322);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(chunk);dc_gpu_destroy(g);PASS();
}
