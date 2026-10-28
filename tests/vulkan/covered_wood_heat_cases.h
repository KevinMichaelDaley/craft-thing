#ifndef COVERED_WOOD_HEAT_CASES_H
#define COVERED_WOOD_HEAT_CASES_H
static void test_covered_wood_conducts_to_its_outer_face_without_leaking_fire(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(uint32_t y=10;y<13;y++) for(uint32_t x=10;x<13;x++) chunk->cells[y*64u+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    double initial=0.;
    for(uint32_t y=10;y<13;y++) for(uint32_t x=10;x<13;x++) {
        fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,y==12u?1200.f:300.f,7000u+y*64u+x,&state,err,sizeof(err)));
        initial+=fr_reactive_state_energy(&state);ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,y*64u+x,&state,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_mark_bonfire_world(g));
    /* Finite initial heat, no gas or combustion: this isolates transport through
     * the actual rigid rest grid, including its initially cold outer layer. */
    for(uint32_t step=0;step<30;step++) ASSERT_TRUE(fr_thermal_gpu_step(g,1.f,err,sizeof(err)));
    double final=0.;float outer=0.;
    for(uint32_t y=0;y<3;y++) for(uint32_t x=0;x<3;x++) {
        fr_reactive_record_t result;bool found=false;float temperature=0.;
        ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,x,y,&result,&found,err,sizeof(err))&&found&&fr_reactive_state_temperature(&result.state,&temperature,err,sizeof(err)));
        final+=fr_reactive_state_energy(&result.state);if(y==0u) outer+=temperature/3.f;
        ASSERT_TRUE((result.state.flags&FR_REACTIVE_FLAMING)==0u);
    }
    printf("Covered wood outer face %.3fK after30s; energy drift%.9gJ\n",outer,final-initial);
    ASSERT_TRUE(outer>301.5f&&outer<306.f&&fabs(final-initial)<.01);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(chunk);dc_gpu_destroy(g);PASS();
}
#endif
