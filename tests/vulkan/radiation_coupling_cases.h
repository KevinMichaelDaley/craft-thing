#ifndef DC_RADIATION_COUPLING_CASES_H
#define DC_RADIATION_COUPLING_CASES_H

#include "dungeoncraft/thermal_surface.h"

static void test_isolated_hot_wood_radiates_to_open_sky(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    ASSERT_TRUE(chunk&&context(&g,err,sizeof(err)));
    chunk->cells[650].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t hot;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,1400,7100,&hot,err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,0,650,&hot,err,sizeof(err))&&dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_mark_bonfire_world(g));
    ASSERT_TRUE(fr_thermal_gpu_step(g,.05f,err,sizeof(err)));
    fr_reactive_record_t after;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&after,&found,err,sizeof(err))&&found);
    double lost=fr_reactive_state_energy(&hot)-fr_reactive_state_energy(&after.state);
    double expected=4.*FR_THERMAL_STEFAN_BOLTZMANN*FR_THERMAL_FACE_AREA*
        FR_THERMAL_SURFACE_EMISSIVITY*(pow(1400.,4.)-pow(FR_THERMAL_AMBIENT_K,4.))*.05;
    printf("Open-sky radiation lost %.6f J, expected %.6f J\n",lost,expected);
    ASSERT_TRUE(lost>expected*.7&&lost<expected*1.3);
    free(chunk);dc_gpu_destroy(g);PASS();
}

static void test_soot_flame_radiates_to_distant_wood_with_a_paired_ledger(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    ASSERT_TRUE(chunk&&context(&g,err,sizeof(err)));
    chunk->cells[653].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t cold;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,300,7200,&cold,err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,0,653,&cold,err,sizeof(err))&&dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&
        fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*output=calloc(4096,sizeof(*output));
    double *volumes=malloc(4096*sizeof(*volumes));ASSERT_TRUE(input&&output&&volumes);
    for(uint32_t i=0;i<4096;i++) volumes[i]=1.;
    input[650].species[FR_GAS_NITROGEN]=.00072;
    input[650].species[FR_GAS_SOOT]=.0002;
    input[650].energy=(.00072*.74320+.0002*.71)*1400.;
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err))&&
        fr_combustion_gpu_step(g,.05f,err,sizeof(err))&&
        fr_gas_gpu_read_chunk(g,0,output,4096,err,sizeof(err)));
    fr_reactive_record_t wood;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&wood,&found,err,sizeof(err))&&found);
    double gained=fr_reactive_state_energy(&wood.state)-fr_reactive_state_energy(&cold);
    double lost=input[650].energy-output[650].energy;
    printf("Flame radiation delivered %.9g J, gas debit %.9g J\n",gained,lost);
    ASSERT_TRUE(gained>.001&&fabs(gained-lost)<.01);
    free(volumes);free(input);free(output);free(chunk);dc_gpu_destroy(g);PASS();
}

#endif
