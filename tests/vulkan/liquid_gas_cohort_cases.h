#include "ferrum/fire/gas_gpu.h"

static void check_gas_cohort(uint32_t policy, bool missing_corner) {
    const char *shader = "build/shaders/pattern.comp.spv";
    const fr_gas_model_t model = {.gas_constant={1,1,1,1,0},.heat_capacity_cv={2.5,2.5,2.5,2.5,1},.kinetic_scale=1};
    dc_gpu_t *owners[2]={0}, *mono=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *states=calloc(DC_CHUNK_CELLS,sizeof(*states));
    fr_gas_checkpoint_chunk_t *actual=malloc(sizeof(*actual)), *reference=malloc(sizeof(*reference));
    double *volumes=malloc(DC_CHUNK_CELLS*sizeof(*volumes));
    dc_liquid_seam_endpoint_t tiles[3]={0};
    unsigned count=missing_corner?3u:2u;
    ASSERT_TRUE(chunk && states && actual && reference && volumes &&
        dc_gpu_create(&owners[0],64,missing_corner?128:64,shader,g_err,sizeof(g_err)) &&
        dc_gpu_create_shared(&owners[1],owners[0],64,missing_corner?128:64,shader,g_err,sizeof(g_err)) &&
        dc_gpu_create(&mono,128,missing_corner?128:64,shader,g_err,sizeof(g_err)));
    for(unsigned i=0;i<count;i++) {
        uint32_t x=i==1u, y=i==2u;
        dc_gpu_t *owner=owners[x];
        chunk->coord=(dc_chunk_coord_t){x,y};
        tiles[i]=(dc_liquid_seam_endpoint_t){.gpu=owner,.coord=chunk->coord,.slot=y,.tile_y=y};
        ASSERT_TRUE((y || dc_gpu_set_body_origin(owner,chunk->coord,g_err,sizeof(g_err))) &&
            dc_gpu_upload_chunk(owner,y,chunk,g_err,sizeof(g_err)) &&
            dc_gpu_set_page(owner,0,y,y,g_err,sizeof(g_err)) &&
            dc_gpu_upload_chunk(mono,i,chunk,g_err,sizeof(g_err)) &&
            dc_gpu_set_page(mono,x,y,i,g_err,sizeof(g_err)));
    }
    ASSERT_TRUE(fr_gas_gpu_enable(mono,&model,1.f,300.f,g_err,sizeof(g_err)) &&
        fr_gas_gpu_set_pressure_policy(mono,policy,g_err,sizeof(g_err)));
    for(unsigned i=0;i<count;i++) {
        dc_gpu_t *owner=tiles[i].gpu;
        for(unsigned j=0;j<DC_CHUNK_CELLS;j++) {
            bool warm=(j/64u<32u)==(i==0u);
            states[j]=(fr_gas_state_t){.species={.2,.8,warm?.1:0.,warm?.03:0.,warm?.02:0.},
                .pigment={warm?.05:0.,0,0}};
            for(unsigned k=0;k<FR_GAS_SPECIES;k++)
                states[j].energy+=states[j].species[k]*model.heat_capacity_cv[k]*(warm?330.:300.);
            volumes[j]=1.;
        }
        ASSERT_TRUE(fr_gas_gpu_enable(owner,&model,1.f,300.f,g_err,sizeof(g_err)) &&
            fr_gas_gpu_set_pressure_policy(owner,policy,g_err,sizeof(g_err)) &&
            fr_gas_gpu_seed_chunk(owner,tiles[i].slot,states,volumes,DC_CHUNK_CELLS,g_err,sizeof(g_err)) &&
            fr_gas_gpu_seed_chunk(mono,i,states,volumes,DC_CHUNK_CELLS,g_err,sizeof(g_err)));
    }
    ASSERT_TRUE(fr_gas_gpu_gate(owners[0],63u,21u,1u,8u,0u,true,g_err,sizeof(g_err)) &&
        fr_gas_gpu_gate(mono,63u,21u,1u,8u,0u,true,g_err,sizeof(g_err)));
    const float dt=.005f;
    ASSERT_TRUE(dc_gpu_set_tick_seconds(mono,dt) && dc_gpu_tick_step(mono,g_err,sizeof(g_err)));
    ASSERT_TRUE(fr_gas_gpu_set_wind(owners[0],1.f,g_err,sizeof(g_err)));
    ASSERT_EQ(dc_gpu_tick_water_cohort(tiles,count,dt,g_err,sizeof(g_err)),DC_LIQUID_SEAM_REJECTED);
    ASSERT_TRUE(fr_gas_gpu_set_wind(owners[0],0.f,g_err,sizeof(g_err)));
    ASSERT_EQ(dc_gpu_tick_water_cohort(tiles,count,dt,g_err,sizeof(g_err)),DC_LIQUID_SEAM_COMMITTED);
    for(unsigned i=0;i<count;i++) {
        dc_gpu_t *owner=tiles[i].gpu;
        ASSERT_TRUE(fr_gas_gpu_export_checkpoint(mono,i,reference,g_err,sizeof(g_err)) &&
            fr_gas_gpu_export_checkpoint(owner,tiles[i].slot,actual,g_err,sizeof(g_err)));
        unsigned receiver=i?16u*64u:48u*64u+63u;
        if (i<2u && policy != FR_GAS_PRESSURE_IMPLICIT) {
            ASSERT_TRUE(reference->cells[receiver][FR_GAS_EXHAUST]>0.f);
            ASSERT_TRUE(reference->cells[receiver][FR_GAS_SOOT]>0.f);
        }
        if (i==1u && policy==FR_GAS_PRESSURE_HLLC) ASSERT_EQ(reference->cells[25u*64u][FR_GAS_EXHAUST],0.f);
        ASSERT_EQ(actual->clock.physical_steps,reference->clock.physical_steps);
        ASSERT_EQ(actual->clock.advanced_seconds,reference->clock.advanced_seconds);
        ASSERT_EQ(memcmp(actual->cells,reference->cells,sizeof(actual->cells)),0);
        ASSERT_EQ(memcmp(actual->carrier_tail,reference->carrier_tail,sizeof(actual->carrier_tail)),0);
        ASSERT_EQ(memcmp(actual->gates,reference->gates,sizeof(actual->gates)),0);
    }
    PASS();
cleanup:
    dc_gpu_destroy(mono);
    dc_gpu_destroy(owners[1]);
    dc_gpu_destroy(owners[0]);
    free(volumes);
    free(reference);
    free(actual);
    free(states);
    free(chunk);
}

static void test_each_gas_species_and_pressure_work_cross_the_owner_face(void) {
    check_gas_cohort(FR_GAS_PRESSURE_HLLC, false);
}
static void test_implicit_gas_pressure_work_matches_across_the_owner_face(void) {
    check_gas_cohort(FR_GAS_PRESSURE_IMPLICIT, false);
}
static void test_practical_gas_transport_matches_across_the_owner_face(void) {
    check_gas_cohort(FR_GAS_PRESSURE_PRACTICAL, false);
}

static void test_gas_domain_preserves_the_unmapped_corner(void) {
    check_gas_cohort(FR_GAS_PRESSURE_HLLC, true);
}
