#ifndef GAS_GEOMETRY_CASES_H
#define GAS_GEOMETRY_CASES_H
#include "../../src/vulkan/gas_gpu_internal.h"
static double gas_checkpoint_stock(const fr_gas_checkpoint_chunk_t *snapshot,uint32_t species) {
    double total=0.;for(uint32_t i=0;i<4096u;i++) total+=snapshot->cells[i][species];return total;
}
static void test_native_valve_only_refresh_preserves_restored_physical_void(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=malloc(4096*sizeof(*input));double *volume=malloc(4096*sizeof(*volume));
    fr_gas_checkpoint_chunk_t *out=malloc(sizeof(*out));ASSERT_TRUE(chunk&&input&&volume&&out);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) {input[i]=reference_air(model,.0012,300,0);volume[i]=.4;}
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_gate(g,20,20,1,1,0,true,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_EQ(out->cells[20u*64u+20u][13],.4f);ASSERT_EQ(out->clock.physical_steps,0u);
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);free(chunk);free(input);free(volume);free(out);PASS();
}
static void test_native_geometry_carving_displacement_and_trapped_pockets_keep_finite_stock(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=malloc(4096*sizeof(*input));double *volume=malloc(4096*sizeof(*volume));
    fr_gas_checkpoint_chunk_t *before=malloc(sizeof(*before)),*out=malloc(sizeof(*out));ASSERT_TRUE(chunk&&input&&volume&&before&&out);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) {
        bool wall=i%64u==32u;volume[i]=wall?0.:1.;input[i]=wall?(fr_gas_state_t){0}:reference_air(model,.0012,300,0);
        if(wall) {chunk->cells[i].material=DC_MATERIAL_STONE;chunk->cells[i].flags=DC_CELL_LOADED_TERRAIN;}
        else if(i%64u<32u) {input[i].species[1]-=.00001;input[i].species[2]=.00001;input[i].pigment[0]=.00001;
            input[i].energy+=.00001*(model.heat_capacity_cv[2]-model.heat_capacity_cv[1])*300;}
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,0,before,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,32,32,0,DC_MATERIAL_AIR,err,sizeof(err)));fr_gas_gpu_geometry_changed(g,false);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_EQ(out->cells[32u*64u+32u][12],1.f);ASSERT_EQ(out->cells[32u*64u+32u][0],0.f);
    for(uint32_t k=0;k<5u;k++) ASSERT_TRUE(fabs(gas_checkpoint_stock(out,k)-gas_checkpoint_stock(before,k))<1e-8);
    for(uint32_t i=0;i<20u;i++) ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err))&&out->cells[32u*64u+32u][0]>0.f);
    double stock=gas_checkpoint_stock(out,0),pigment=gas_checkpoint_stock(out,8);
    ASSERT_TRUE(dc_gpu_paint_material(g,20,20,0,DC_MATERIAL_STONE,err,sizeof(err)));fr_gas_gpu_geometry_changed(g,false);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_EQ(out->cells[20u*64u+20u][12],0.f);ASSERT_EQ(out->cells[20u*64u+20u][0],0.f);
    ASSERT_TRUE(fabs(gas_checkpoint_stock(out,0)-stock)<stock*1e-5&&fabs(gas_checkpoint_stock(out,8)-pigment)<pigment*1e-5);
    ASSERT_TRUE(dc_gpu_paint_material(g,12,12,1,DC_MATERIAL_STONE,err,sizeof(err)));fr_gas_gpu_geometry_changed(g,false);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_TRUE(out->cells[12u*64u+12u][0]>0.f&&out->cells[12u*64u+12u][12]>0.f&&out->cells[12u*64u+12u][13]==0.f);
    float trapped_oxygen=out->cells[12u*64u+12u][0];
    for(uint32_t i=0;i<10u;i++) ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));ASSERT_EQ(out->cells[12u*64u+12u][0],trapped_oxygen);
    ASSERT_TRUE(fr_gas_gpu_gate(g,12,12,1,1,0,true,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,13,12,0,DC_MATERIAL_AIR,err,sizeof(err)));fr_gas_gpu_geometry_changed(g,false);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_EQ(out->cells[12u*64u+12u][0],trapped_oxygen);
    ASSERT_TRUE(fr_gas_gpu_gate(g,12,12,1,1,0,false,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_EQ(out->cells[12u*64u+12u][0],0.f);ASSERT_EQ(out->cells[12u*64u+12u][12],0.f);
    double original=gas_checkpoint_stock(before,0),final=gas_checkpoint_stock(out,0);
    printf("Geometry oxygen drift%.9g relative%.9g; trapped pocket retained and released\n",final-original,(final-original)/original);
    ASSERT_TRUE(fabs(final-original)<original*1e-4);
    ASSERT_EQ(out->clock.physical_steps,30u);ASSERT_TRUE(fabs(out->clock.advanced_seconds-.6)<1e-7);
    for(uint32_t i=0;i<4096u;i++) for(uint32_t k=0;k<16u;k++) ASSERT_TRUE(isfinite(out->cells[i][k]));
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);free(chunk);free(input);free(volume);free(before);free(out);PASS();
}
#endif
