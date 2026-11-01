#ifndef GAS_GEOMETRY_CASES_H
#define GAS_GEOMETRY_CASES_H
#include "../../src/vulkan/gas_gpu_internal.h"
#include "ferrum/fire/bulk_phase_gpu.h"
static double gas_checkpoint_stock(const fr_gas_checkpoint_chunk_t *snapshot,uint32_t species) {
    double total=0.;for(uint32_t i=0;i<4096u;i++) total+=snapshot->cells[i][species];return total;
}
static double gas_checkpoint_energy(const fr_gas_checkpoint_chunk_t *snapshot) {
    double total=0.;
    for(uint32_t i=0;i<4096u;i++)
        total+=(double)snapshot->cells[i][5]+snapshot->cells[i][11]+snapshot->cells[i][14];
    return total;
}
static void test_mpm_cut_cell_wedge_vents_stock_without_pressure_runaway(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*out=calloc(4096,sizeof(*out));
    double *volume=calloc(4096,sizeof(*volume));
    fr_gas_checkpoint_chunk_t *snapshot=malloc(sizeof(*snapshot));
    ASSERT_TRUE(chunk&&input&&out&&volume&&snapshot);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) chunk->cells[i].material=DC_MATERIAL_STONE;
    for(uint32_t x=16u;x<=30u;x++) {
        uint32_t i=32u*64u+x;
        chunk->cells[i].material=DC_MATERIAL_AIR;
        volume[i]=x<=20u?1.:1./(double)(1u<<(x-19u));
        input[i]=reference_air(model,x<=20u?.0012:.00005,300,0);
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&
        fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    for(uint32_t x=21u;x<=30u;x++)
        ((float *)g->mpm_grid_mapped)[(32u*g->width+x)*4u+3u]=1.f-(float)volume[32u*64u+x];
    fr_gas_gpu_geometry_changed(g,true);
    double before=0.;for(uint32_t x=16u;x<=30u;x++) before+=input[32u*64u+x].species[0];
    for(uint32_t frame=0;frame<60u;frame++) ASSERT_TRUE(fr_gas_gpu_step(g,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,snapshot,err,sizeof(err)));
    double after=0.,peak=0.;
    for(uint32_t x=16u;x<=30u;x++) {
        uint32_t i=32u*64u+x;after+=out[i].species[0];
        fr_gas_thermo_t t;
        ASSERT_TRUE(fr_gas_thermodynamics(&model,out+i,snapshot->cells[i][12],&t,err,sizeof(err)));
        if(t.pressure>peak) peak=t.pressure;
    }
    printf("MPM wedge pressure %.9g oxygen drift %.9g\n",peak,after-before);
    ASSERT_TRUE(fabs(after-before)<1e-7&&peak<500.);
    dc_gpu_destroy(g);free(chunk);free(input);free(out);free(volume);free(snapshot);PASS();
}
static void test_pressurized_pocket_pushes_mobile_sand_without_losing_oxygen(void) {
    char err[256]={0};dc_gpu_t *grids[2]={0};
    dc_chunk_t *chunks[2]={calloc(1,sizeof(dc_chunk_t)),calloc(1,sizeof(dc_chunk_t))};
    dc_chunk_t *after=calloc(1,sizeof(*after));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*readback=calloc(4096,sizeof(*readback));
    double *volume=calloc(4096,sizeof(*volume));
    ASSERT_TRUE(chunks[0]&&chunks[1]&&after&&input&&readback&&volume);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) {input[i]=reference_air(model,.0012,300,0);volume[i]=1.;}
    const uint32_t grain=30u*64u+21u,source=30u*64u+20u;
    for(uint32_t variant=0;variant<2u;variant++) {
        dc_chunk_t *chunk=chunks[variant];
        for(uint32_t x=19u;x<=23u;x++) {
            chunk->cells[29u*64u+x].material=DC_MATERIAL_STONE;
            chunk->cells[31u*64u+x].material=DC_MATERIAL_STONE;
        }
        chunk->cells[30u*64u+19u].material=DC_MATERIAL_STONE;
        chunk->cells[grain].material=DC_MATERIAL_SAND;
        dc_chunk_seed_particles(chunk);
        ASSERT_TRUE(dc_gpu_create(&grids[variant],64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_upload_chunk(grids[variant],0,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(grids[variant],0,0,0,err,sizeof(err)));
        bool enabled=fr_gas_gpu_enable(grids[variant],&model,.0012,300,err,sizeof(err));
        if(!enabled) printf("MPM pressure setup: %s\n",err);
        ASSERT_TRUE(enabled);
        ASSERT_TRUE(fr_gas_gpu_set_pressure_policy(grids[variant],FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
        if(variant==1u) input[source]=reference_air(model,.02,300,0);
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(grids[variant],0,input,volume,4096,err,sizeof(err)));
    }
    for(uint32_t frame=0;frame<30u;frame++) for(uint32_t variant=0;variant<2u;variant++)
        ASSERT_TRUE(dc_gpu_tick_step(grids[variant],err,sizeof(err)));
    double positions[2]={0};
    for(uint32_t variant=0;variant<2u;variant++) {
        ASSERT_TRUE(dc_gpu_download_chunk(grids[variant],0,after,err,sizeof(err)));
        bool found=false;
        for(uint32_t i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++)
            if(after->particles[i].mass_fp&&after->particles[i].id_lo==grain+1u) {
                positions[variant]=(double)after->particles[i].x_fp/65536.;found=true;break;
            }
        ASSERT_TRUE(found);
    }
    ASSERT_TRUE(positions[1]>positions[0]+.01);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(grids[1],0,readback,4096,err,sizeof(err)));
    double oxygen=0.;for(uint32_t i=0;i<4096u;i++) oxygen+=readback[i].species[0];
    double expected=4095.*reference_air(model,.0012,300,0).species[0]+
        reference_air(model,.02,300,0).species[0];
    ASSERT_TRUE(fabs(oxygen-expected)<expected*.003);
    for(uint32_t i=0;i<2u;i++) dc_gpu_destroy(grids[i]);
    free(chunks[0]);free(chunks[1]);free(after);free(input);free(readback);free(volume);PASS();
}
static void test_sealed_mpm_compression_records_reversible_gas_work(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=calloc(4096,sizeof(*input));double *volume=calloc(4096,sizeof(*volume));
    fr_gas_checkpoint_chunk_t *out=malloc(sizeof(*out));
    ASSERT_TRUE(chunk&&input&&volume&&out);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) chunk->cells[i].material=DC_MATERIAL_STONE;
    uint32_t center=32u*64u+32u;
    chunk->cells[center].material=DC_MATERIAL_AIR;
    input[center]=reference_air(model,.0012,300,0);volume[center]=1.;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&
        fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    ((float *)g->mpm_grid_mapped)[center*4u+3u]=.5f;
    fr_gas_gpu_geometry_changed(g,true);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    fr_gas_state_t expected;double work=0.;
    ASSERT_TRUE(fr_gas_reference_compress(&model,input+center,1.,.5,&expected,&work,err,sizeof(err)));
    double energy=(double)out->cells[center][5]+out->cells[center][11]+out->cells[center][14];
    ASSERT_TRUE(fabs(out->cells[center][12]-.5f)<1e-7);
    ASSERT_TRUE(fabs(energy-expected.energy)<1e-5&&fabs(out->cells[center][15]-work)<1e-5);
    ((float *)g->mpm_grid_mapped)[center*4u+3u]=0.f;
    fr_gas_gpu_geometry_changed(g,true);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    energy=(double)out->cells[center][5]+out->cells[center][11]+out->cells[center][14];
    ASSERT_TRUE(fabs(out->cells[center][12]-1.f)<1e-7);
    ASSERT_TRUE(fabs(energy-input[center].energy)<1e-5&&fabs(out->cells[center][15])<1e-5);
    ASSERT_TRUE(fabs(out->cells[center][0]-input[center].species[0])<1e-8);
    dc_gpu_destroy(g);free(chunk);free(input);free(volume);free(out);PASS();
}
static void test_dense_granular_mpm_retains_connected_pore_volume(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=calloc(4096,sizeof(*input));double *volume=calloc(4096,sizeof(*volume));
    fr_gas_checkpoint_chunk_t *out=malloc(sizeof(*out));
    ASSERT_TRUE(chunk&&input&&volume&&out);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    uint32_t center=32u*64u+32u;
    for(uint32_t i=0;i<4096u;i++) {input[i]=reference_air(model,.0012,300,0);volume[i]=1.;}
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&
        fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    ((float *)g->mpm_grid_mapped)[center*4u+3u]=.999f;
    fr_gas_gpu_geometry_changed(g,true);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    printf("Dense MPM pore volume %.9g\n",out->cells[center][12]);
    ASSERT_TRUE(out->cells[center][12]>=.35f);
    ASSERT_TRUE(fabs(gas_checkpoint_stock(out,FR_GAS_OXYGEN)-
        input[0].species[FR_GAS_OXYGEN]*4096.)<1e-5);
    dc_gpu_destroy(g);free(chunk);free(input);free(volume);free(out);PASS();
}
static void test_rigid_sweep_routes_gas_through_newly_covered_cells(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=calloc(4096,sizeof(*input));double *volume=calloc(4096,sizeof(*volume));
    fr_gas_checkpoint_chunk_t *out=malloc(sizeof(*out));
    ASSERT_TRUE(chunk&&input&&volume&&out);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0u;i<4096u;i++) chunk->cells[i].material=DC_MATERIAL_STONE;
    for(uint32_t x=20u;x<=26u;x++) {
        uint32_t i=32u*64u+x;
        chunk->cells[i].material=DC_MATERIAL_AIR;
        input[i]=reference_air(model,.0012,300,0);
        volume[i]=1.;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&
        fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    uint32_t cells=g->width*g->height;
    for(uint32_t x=22u;x<=24u;x++)
        ((uint32_t *)g->occupancy_mapped)[2u*cells+32u*g->width+x]=65536u;
    fr_gas_gpu_geometry_changed(g,true);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    double oxygen=0.;
    for(uint32_t x=20u;x<=26u;x++) oxygen+=out->cells[32u*64u+x][FR_GAS_OXYGEN];
    uint32_t center=32u*64u+23u;
    printf("Rigid sweep center stock %.9g volume %.9g oxygen drift %.9g\n",
        out->cells[center][FR_GAS_OXYGEN],out->cells[center][12],
        oxygen-7.*input[center].species[FR_GAS_OXYGEN]);
    ASSERT_TRUE(fabs(oxygen-7.*input[center].species[FR_GAS_OXYGEN])<1e-7);
    ASSERT_TRUE(out->cells[center][FR_GAS_OXYGEN]<1e-10f);
    ASSERT_TRUE(out->cells[center][12]<1e-7f);
    dc_gpu_destroy(g);free(chunk);free(input);free(volume);free(out);PASS();
}
static void test_rigid_wake_fills_through_newly_opened_cells(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=calloc(4096,sizeof(*input));double *volume=calloc(4096,sizeof(*volume));
    fr_gas_checkpoint_chunk_t *out=malloc(sizeof(*out));
    ASSERT_TRUE(chunk&&input&&volume&&out);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0u;i<4096u;i++) chunk->cells[i].material=DC_MATERIAL_STONE;
    for(uint32_t x=20u;x<=26u;x++) chunk->cells[32u*64u+x].material=DC_MATERIAL_AIR;
    for(uint32_t x=20u;x<=26u;x++) {
        uint32_t i=32u*64u+x;
        if(x>=22u&&x<=24u) continue;
        input[i]=reference_air(model,.0012,300,0);volume[i]=1.;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&
        fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    fr_gas_gpu_geometry_changed(g,true);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    double oxygen=0.;for(uint32_t x=20u;x<=26u;x++) oxygen+=out->cells[32u*64u+x][FR_GAS_OXYGEN];
    uint32_t center=32u*64u+23u;
    printf("Rigid wake center stock %.9g volume %.9g oxygen drift %.9g\n",
        out->cells[center][FR_GAS_OXYGEN],out->cells[center][12],
        oxygen-4.*input[32u*64u+20u].species[FR_GAS_OXYGEN]);
    ASSERT_TRUE(fabs(oxygen-4.*input[32u*64u+20u].species[FR_GAS_OXYGEN])<1e-7);
    ASSERT_TRUE(out->cells[center][FR_GAS_OXYGEN]>1e-7f);
    ASSERT_TRUE(out->cells[center][12]>.99f);
    dc_gpu_destroy(g);free(chunk);free(input);free(volume);free(out);PASS();
}
static void test_burning_wood_has_small_conservative_smoke_bypass(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*out=calloc(4096,sizeof(*out));
    double *volume=calloc(4096,sizeof(*volume));
    fr_gas_checkpoint_chunk_t *snapshot=malloc(sizeof(*snapshot));
    ASSERT_TRUE(chunk&&input&&out&&volume&&snapshot);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) chunk->cells[i].material=DC_MATERIAL_STONE;
    uint32_t left=32u*64u+30u,wood=left+1u,right=wood+1u;
    chunk->cells[left].material=DC_MATERIAL_AIR;
    chunk->cells[wood].material=DC_MATERIAL_WOOD;
    chunk->cells[wood].flags=0x8000u;
    chunk->cells[right].material=DC_MATERIAL_AIR;
    input[left]=reference_air(model,.0012,300,0);
    input[left].species[FR_GAS_NITROGEN]-=.0001;
    input[left].species[FR_GAS_SOOT]+=.0001;
    input[left].pigment[0]=.00008;
    input[left].energy+=.0001*(model.heat_capacity_cv[FR_GAS_SOOT]-model.heat_capacity_cv[FR_GAS_NITROGEN])*300.;
    input[right]=reference_air(model,.0012,300,0);
    volume[left]=volume[right]=1.;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&
        fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    fr_gas_gpu_geometry_changed(g,true);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,snapshot,err,sizeof(err)));
    ASSERT_TRUE(snapshot->cells[wood][12]>.05f&&snapshot->cells[wood][12]<.25f);
    double initial=input[left].species[FR_GAS_SOOT];
    for(uint32_t frame=0;frame<60u;frame++) ASSERT_TRUE(fr_gas_gpu_step(g,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    ASSERT_TRUE(out[right].species[FR_GAS_SOOT]>0.);
    ASSERT_TRUE(fabs(out[left].species[FR_GAS_SOOT]+out[wood].species[FR_GAS_SOOT]+
        out[right].species[FR_GAS_SOOT]-initial)<1e-7);
    chunk->cells[wood].flags=0u;
    ASSERT_TRUE(dc_gpu_upload_chunk_exact(g,0,chunk,false,err,sizeof(err)));
    fr_gas_gpu_geometry_changed(g,true);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,snapshot,err,sizeof(err))&&
        fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    ASSERT_EQ(snapshot->cells[wood][12],0.f);
    ASSERT_TRUE(fabs(out[left].species[FR_GAS_SOOT]+out[wood].species[FR_GAS_SOOT]+
        out[right].species[FR_GAS_SOOT]-initial)<1e-7);
    dc_gpu_destroy(g);free(chunk);free(input);free(out);free(volume);free(snapshot);PASS();
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
static void test_native_oil_volume_excludes_gas_and_routes_oxygen(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=calloc(4096,sizeof(*input));double *volume=calloc(4096,sizeof(*volume));
    fr_bulk_phase_chunk_t *bulk=calloc(1,sizeof(*bulk));
    fr_gas_checkpoint_chunk_t *out=malloc(sizeof(*out));ASSERT_TRUE(chunk&&input&&volume&&bulk&&out);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) {input[i]=reference_air(model,.0012,300,0);volume[i]=1.;}
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&
        fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    const uint32_t center=32u*64u+32u;bulk->oil_fp[center]=FR_BULK_CELL_FULL_FP;
    ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(g,0,chunk,bulk,err,sizeof(err))&&
        fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_EQ(out->cells[center][12],0.f);ASSERT_EQ(out->cells[center][0],0.f);
    ASSERT_TRUE(out->cells[center+1u][0]>input[center+1u].species[0]);
    ASSERT_TRUE(fabs(gas_checkpoint_stock(out,FR_GAS_OXYGEN)-
        input[0].species[FR_GAS_OXYGEN]*4096.)<1e-5);
    bulk->oil_fp[center]=FR_BULK_CELL_FULL_FP/2u;
    ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(g,0,chunk,bulk,err,sizeof(err))&&
        fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_EQ(out->cells[center][12],.5f);
    ASSERT_TRUE(out->cells[center][0]>0.f);
    ASSERT_TRUE(fabs(gas_checkpoint_stock(out,FR_GAS_OXYGEN)-
        input[0].species[FR_GAS_OXYGEN]*4096.)<1e-5);
    dc_gpu_destroy(g);free(chunk);free(input);free(volume);free(bulk);free(out);PASS();
}
static void test_native_quantized_sliver_routes_gas_to_open_neighbor(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=calloc(4096,sizeof(*input));double *volume=calloc(4096,sizeof(*volume));
    fr_gas_checkpoint_chunk_t *out=malloc(sizeof(*out));ASSERT_TRUE(chunk&&input&&volume&&out);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) {input[i]=reference_air(model,.0012,300,0);volume[i]=1.;}
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&
        fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    const uint32_t center=32u*64u+32u;
    chunk->cells[center].fluid_mass=65535u;
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_EQ(out->cells[center][12],1.f/65536.f);
    ASSERT_TRUE(out->cells[center][0]>0.f&&out->cells[center][0]<input[center].species[0]/65535.f);
    ASSERT_TRUE(fabs(gas_checkpoint_stock(out,0)-.0012*4096.*
        input[0].species[0]/.0012)<1e-5);
    ASSERT_TRUE(out->cells[center+1u][0]>input[center+1u].species[0]);
    dc_gpu_destroy(g);free(chunk);free(input);free(volume);free(out);PASS();
}
static void test_native_geometry_carving_and_multi_cell_displacement_keep_finite_stock(void) {
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
    ASSERT_EQ(out->cells[32u*64u+32u][12],1.f);
    ASSERT_TRUE(out->cells[32u*64u+32u][0]>0.f);
    ASSERT_TRUE(out->cells[32u*64u+31u][0]<before->cells[32u*64u+31u][0]);
    ASSERT_TRUE(fabs(gas_checkpoint_energy(out)-gas_checkpoint_energy(before))<1e-5);
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
    ASSERT_EQ(out->cells[12u*64u+12u][0],0.f);
    ASSERT_EQ(out->cells[12u*64u+12u][12],0.f);
    ASSERT_TRUE(out->cells[12u*64u+14u][0]>0.f);
    for(uint32_t i=0;i<10u;i++) ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,13,12,0,DC_MATERIAL_AIR,err,sizeof(err)));fr_gas_gpu_geometry_changed(g,false);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_EQ(out->cells[12u*64u+12u][0],0.f);ASSERT_EQ(out->cells[12u*64u+12u][12],0.f);
    double original=gas_checkpoint_stock(before,0),final=gas_checkpoint_stock(out,0);
    printf("Geometry oxygen drift%.9g relative%.9g; multi-cell displacement and carve\n",final-original,(final-original)/original);
    ASSERT_TRUE(fabs(final-original)<original*1e-4);
    ASSERT_EQ(out->clock.physical_steps,30u);ASSERT_TRUE(fabs(out->clock.advanced_seconds-.6)<1e-7);
    for(uint32_t i=0;i<4096u;i++) for(uint32_t k=0;k<16u;k++) ASSERT_TRUE(isfinite(out->cells[i][k]));
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);free(chunk);free(input);free(volume);free(before);free(out);PASS();
}
static void test_native_fully_submerged_pocket_retains_stock_against_adjacent_water(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=calloc(4096,sizeof(*input));double *volume=calloc(4096,sizeof(*volume));
    fr_gas_checkpoint_chunk_t *out=malloc(sizeof(*out));ASSERT_TRUE(chunk&&input&&volume&&out);
    const uint32_t at=20u*64u+20u;
    chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WATER,.fluid_mass=DC_FLUID_FULL};
    const uint32_t walls[]={at-1u,at+1u,at-64u,at+64u};
    for(uint32_t k=0;k<4u;k++) chunk->cells[walls[k]].material=DC_MATERIAL_STONE;
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) {
        bool wall=false;for(uint32_t k=0;k<4u;k++) wall|=i==walls[k];
        volume[i]=wall?0.:1.;input[i]=wall?(fr_gas_state_t){0}:reference_air(model,.0012,300,0);
    }
    volume[at]=.01;input[at]=reference_air(model,.012,500,0);
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_thermal_gpu_seed_water(g,0,at,1.f,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&
        fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    fr_gas_gpu_geometry_changed(g,false);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_EQ(out->cells[at][12],.01f);
    ASSERT_EQ(out->cells[at][13],0.f);
    for(uint32_t k=0;k<5u;k++) ASSERT_TRUE(fabs(out->cells[at][k]-input[at].species[k])<1e-9);
    chunk->cells[at+1u]=(dc_cell_t){.material=DC_MATERIAL_WATER,.fluid_mass=DC_FLUID_FULL/2u};
    ASSERT_TRUE(dc_gpu_upload_chunk_exact(g,0,chunk,false,err,sizeof(err)));
    fr_gas_gpu_geometry_changed(g,false);
    ASSERT_TRUE(fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err))&&
        dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_EQ(out->cells[at][13],0.f);
    ASSERT_EQ(chunk->cells[at].fluid_mass,DC_FLUID_FULL);
    ASSERT_EQ(chunk->cells[at+1u].fluid_mass,DC_FLUID_FULL/2u);
    float *water_heat=calloc(4096u,sizeof(*water_heat));ASSERT_TRUE(water_heat);
    ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(g,0,water_heat,4096,err,sizeof(err)));
    double before_gas=gas_checkpoint_energy(out),before_water=0.;
    for(uint32_t i=0;i<4096u;i++) before_water+=water_heat[i];
    double before_energy=before_gas+before_water;
    double before_oxygen=gas_checkpoint_stock(out,FR_GAS_OXYGEN);
    ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err))&&
        dc_gpu_download_chunk(g,0,chunk,err,sizeof(err))&&
        fr_gas_gpu_export_checkpoint(g,0,out,err,sizeof(err)));
    ASSERT_TRUE(chunk->cells[at].fluid_mass<DC_FLUID_FULL);
    ASSERT_TRUE(chunk->cells[at+1u].fluid_mass>DC_FLUID_FULL/2u);
    ASSERT_TRUE(chunk->cells[at+1u].fluid_mass<DC_FLUID_FULL);
    uint64_t water_stock=0;
    for(uint32_t i=0;i<4096u;i++) water_stock+=chunk->cells[i].fluid_mass;
    ASSERT_EQ(water_stock,DC_FLUID_FULL+DC_FLUID_FULL/2u);
    ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(g,0,water_heat,4096,err,sizeof(err)));
    double after_water=0.;for(uint32_t i=0;i<4096u;i++) after_water+=water_heat[i];
    double after_energy=gas_checkpoint_energy(out)+after_water;
    printf("Submerged pocket energy drift %.9g kJ, water %u -> %u Q16; gas %.9g -> %.9g water %.9g -> %.9g work %.9g\n",
        after_energy-before_energy,DC_FLUID_FULL,chunk->cells[at].fluid_mass,
        before_gas,gas_checkpoint_energy(out),before_water,after_water,
        out->cells[at][15]+out->cells[at+1u][15]);
    ASSERT_TRUE(fabs(after_energy-before_energy)<.05);
    ASSERT_TRUE(fabs(gas_checkpoint_stock(out,FR_GAS_OXYGEN)-before_oxygen)<1e-7);
    uint32_t source_water=chunk->cells[at].fluid_mass,receiver_water=chunk->cells[at+1u].fluid_mass;
    ASSERT_TRUE(fr_gas_gpu_step(g,0.f,err,sizeof(err))&&
        dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_EQ(chunk->cells[at].fluid_mass,source_water);
    ASSERT_EQ(chunk->cells[at+1u].fluid_mass,receiver_water);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    for(uint32_t i=0;i<4096u;i++) volume[i]=out->cells[i][12];
    input[at].species[FR_GAS_EXHAUST]+=.001;
    input[at].energy+=.001*model.heat_capacity_cv[FR_GAS_EXHAUST]*500.;
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    fr_gas_gpu_geometry_changed(g,true);
    ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err))&&
        dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(chunk->cells[at].fluid_mass<source_water);
    water_stock=0;
    for(uint32_t i=0;i<4096u;i++) water_stock+=chunk->cells[i].fluid_mass;
    ASSERT_EQ(water_stock,DC_FLUID_FULL+DC_FLUID_FULL/2u);
    free(water_heat);free(out);free(volume);free(input);free(chunk);dc_gpu_destroy(g);PASS();
}
#endif
