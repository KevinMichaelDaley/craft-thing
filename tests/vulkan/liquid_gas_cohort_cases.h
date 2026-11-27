#include "ferrum/fire/gas_gpu.h"

static void test_each_gas_species_and_pressure_work_cross_the_owner_face(void) {
    const char *shader = "build/shaders/pattern.comp.spv";
    const fr_gas_model_t model = {.gas_constant={1,1,1,1,0},.heat_capacity_cv={2.5,2.5,2.5,2.5,1},.kinetic_scale=1};
    dc_gpu_t *owners[2]={0}, *mono=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *states=calloc(DC_CHUNK_CELLS,sizeof(*states));
    fr_gas_checkpoint_chunk_t *actual=malloc(sizeof(*actual)), *reference=malloc(sizeof(*reference));
    double *volumes=malloc(DC_CHUNK_CELLS*sizeof(*volumes));
    dc_liquid_seam_endpoint_t tiles[2]={0};
    ASSERT_TRUE(chunk && states && actual && reference && volumes &&
        dc_gpu_create(&owners[0],64,64,shader,g_err,sizeof(g_err)) &&
        dc_gpu_create_shared(&owners[1],owners[0],64,64,shader,g_err,sizeof(g_err)) &&
        dc_gpu_create(&mono,128,64,shader,g_err,sizeof(g_err)));
    for(unsigned i=0;i<2;i++) {
        chunk->coord.x=i;
        tiles[i]=(dc_liquid_seam_endpoint_t){.gpu=owners[i],.coord=chunk->coord};
        ASSERT_TRUE(dc_gpu_set_body_origin(owners[i],chunk->coord,g_err,sizeof(g_err)) &&
            dc_gpu_upload_chunk(owners[i],0,chunk,g_err,sizeof(g_err)) &&
            dc_gpu_set_page(owners[i],0,0,0,g_err,sizeof(g_err)) &&
            dc_gpu_upload_chunk(mono,i,chunk,g_err,sizeof(g_err)) &&
            dc_gpu_set_page(mono,i,0,i,g_err,sizeof(g_err)));
    }
    ASSERT_TRUE(fr_gas_gpu_enable(mono,&model,1.f,300.f,g_err,sizeof(g_err)) &&
        fr_gas_gpu_set_pressure_policy(mono,FR_GAS_PRESSURE_HLLC,g_err,sizeof(g_err)));
    for(unsigned i=0;i<2;i++) {
        for(unsigned j=0;j<DC_CHUNK_CELLS;j++) {
            bool warm=(j/64u<32u)==(i==0u);
            states[j]=(fr_gas_state_t){.species={.2,.8,warm?.1:0.,warm?.03:0.,warm?.02:0.},
                .pigment={warm?.05:0.,0,0}};
            for(unsigned k=0;k<FR_GAS_SPECIES;k++)
                states[j].energy+=states[j].species[k]*model.heat_capacity_cv[k]*(warm?330.:300.);
            volumes[j]=1.;
        }
        ASSERT_TRUE(fr_gas_gpu_enable(owners[i],&model,1.f,300.f,g_err,sizeof(g_err)) &&
            fr_gas_gpu_set_pressure_policy(owners[i],FR_GAS_PRESSURE_HLLC,g_err,sizeof(g_err)) &&
            fr_gas_gpu_seed_chunk(owners[i],0,states,volumes,DC_CHUNK_CELLS,g_err,sizeof(g_err)) &&
            fr_gas_gpu_seed_chunk(mono,i,states,volumes,DC_CHUNK_CELLS,g_err,sizeof(g_err)));
    }
    const float dt=.005f;
    ASSERT_TRUE(dc_gpu_set_tick_seconds(mono,dt) && dc_gpu_tick_step(mono,g_err,sizeof(g_err)));
    ASSERT_EQ(dc_gpu_tick_water_cohort(tiles,2,dt,g_err,sizeof(g_err)),DC_LIQUID_SEAM_COMMITTED);
    for(unsigned i=0;i<2;i++) {
        ASSERT_TRUE(fr_gas_gpu_export_checkpoint(mono,i,reference,g_err,sizeof(g_err)) &&
            fr_gas_gpu_export_checkpoint(owners[i],0,actual,g_err,sizeof(g_err)));
        unsigned receiver=i?16u*64u:48u*64u+63u;
        ASSERT_TRUE(reference->cells[receiver][FR_GAS_EXHAUST]>0.f);
        ASSERT_TRUE(reference->cells[receiver][FR_GAS_SOOT]>0.f);
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
