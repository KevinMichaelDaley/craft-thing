#define _POSIX_C_SOURCE 200809L
#define main bonfire_fixture_main
#include "../vulkan/reactive_cell_gpu_tests.c"
#undef main
#include "../../src/app/level.c"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "dungeoncraft/combustion_parameters.h"
#include <sys/stat.h>
static void test_bonfire_is_a_playable_finite_terrain_scene(void) {
    char err[256]={0},directory[]="build/bonfire_display_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create_bonfire(directory,7,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(fr_combustion_gpu_enabled(view->gpu));
    uint32_t slot;ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&slot));
    uint32_t cell=16u*64u+28u;fr_reactive_record_t original;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,cell,&original,&found,err,sizeof(err))&&found);
    ASSERT_EQ(original.state.material,DC_MATERIAL_WOOD);ASSERT_TRUE(original.state.fuel_mass_fp>0);
    ASSERT_TRUE(dc_level_view_heat_probe(view,156,208,true));
    for(uint32_t i=0;i<360;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_heat_probe(view,156,208,false));
    for(uint32_t i=0;i<300;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    fr_reactive_record_t burned;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,cell,&burned,&found,err,sizeof(err))&&found);
    float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(&burned.state,&temperature,err,sizeof(err)));
    printf("Bonfire contact: temperature %.3f K fuel %u -> %u hazard %.5f / %.5f\n",temperature,
        original.state.fuel_mass_fp,burned.state.fuel_mass_fp,burned.state.ignition_integral,burned.state.ignition_threshold);
    ASSERT_TRUE(burned.state.fuel_mass_fp<original.state.fuel_mass_fp);
    ASSERT_TRUE((burned.state.flags&FR_REACTIVE_FLAMING)!=0u);
    fr_thermal_inspection_t warmth;
    ASSERT_TRUE(fr_thermal_gpu_inspect(view->gpu,220,271,&warmth,err,sizeof(err)));
    ASSERT_TRUE(warmth.occupied&&warmth.material==DC_MATERIAL_AIR&&warmth.temperature_K>293.16f);
    printf("Measured air warmth %.3f K\n",warmth.temperature_K);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels);
    ASSERT_TRUE(dc_gpu_readback(view->gpu,pixels,480u*270u,err,sizeof(err)));
    mkdir("build/screenshots",0777);
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);
    ASSERT_TRUE(image&&SDL_SaveBMP(image,"build/screenshots/ordinary_bonfire.bmp")==0);
    SDL_FreeSurface(image);
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas));dc_chunk_t *physical=calloc(1,sizeof(*physical));ASSERT_TRUE(gas&&physical);
    fr_combustion_emission_t *emission=calloc(4096,sizeof(*emission));ASSERT_TRUE(emission);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(view->gpu,slot,gas,4096,err,sizeof(err))&&dc_gpu_download_chunk(view->gpu,slot,physical,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_read_emission_chunk(view->gpu,slot,emission,4096,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    uint32_t flame_cells=0;double hot_carbon=0,max_carbon_K=0,max_air_K=0,max_power=0,power_oxygen=0;uint32_t power_pixel=0;
    for(uint32_t i=0;i<4096;i++) {
        if(physical->cells[i].material!=DC_MATERIAL_AIR||physical->cells[i].fluid_mass) continue;
        fr_gas_thermo_t local;ASSERT_TRUE(fr_gas_thermodynamics(&model,gas+i,1,&local,err,sizeof(err)));
        if(local.temperature_K>max_air_K) max_air_K=local.temperature_K;
        if(gas[i].species[FR_GAS_SOOT]>1e-12) {
            if(local.temperature_K>max_carbon_K) max_carbon_K=local.temperature_K;
            if(local.temperature_K>FR_COMBUSTION_AIRBORNE_IGNITION_K) hot_carbon+=gas[i].species[FR_GAS_SOOT];
            double oxygen=local.mass>0?gas[i].species[FR_GAS_OXYGEN]/local.mass:0;
            double activity=FR_COMBUSTION_AIRBORNE_RATE*fmin(1,fmax(0,(local.temperature_K-FR_COMBUSTION_AIRBORNE_IGNITION_K)/
                (FR_COMBUSTION_AIRBORNE_FULL_RATE_K-FR_COMBUSTION_AIRBORNE_IGNITION_K)))*
                fmin(1,fmax(0,(oxygen-FR_COMBUSTION_EXTINGUISH_OXYGEN)/(FR_COMBUSTION_AMBIENT_OXYGEN-FR_COMBUSTION_EXTINGUISH_OXYGEN)));
            double power=activity*gas[i].species[FR_GAS_SOOT]*FR_COMBUSTION_AIRBORNE_ENERGY;
            if(power>max_power) {max_power=power;power_oxygen=oxygen;power_pixel=pixels[(192u+i/64u)*480u+128u+i%64u];}
        }
        uint32_t pixel=pixels[(192u+i/64u)*480u+128u+i%64u],r=pixel&255u,g=(pixel>>8)&255u,b=(pixel>>16)&255u;
        if(r>100u&&r>g+30u&&r>2u*b) {
            fr_gas_thermo_t thermo;ASSERT_TRUE(fr_gas_thermodynamics(&model,gas+i,1,&thermo,err,sizeof(err)));
            ASSERT_TRUE(emission[i].power>0&&emission[i].sources!=0&&emission[i].temperature_K>=FR_COMBUSTION_AIRBORNE_IGNITION_K);
            flame_cells++;
        }
    }
    printf("Actual local gas flame cells %u, max air %.3f K max carbon %.3f K hot carbon %.9g\n",flame_cells,max_air_K,max_carbon_K,hot_carbon);
    printf("Peak local reaction power %.9g oxygen %.6f display rgba %08x\n",max_power,power_oxygen,power_pixel);
    ASSERT_TRUE(flame_cells>0);free(gas);free(physical);free(emission);
    uint32_t *paused=malloc(480u*270u*4u);ASSERT_TRUE(paused);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_gpu_readback(view->gpu,paused,480u*270u,err,sizeof(err)));
    ASSERT_EQ(memcmp(pixels,paused,480u*270u*4u),0);free(pixels);free(paused);
    fr_thermal_probe_t source,restored_source;
    ASSERT_TRUE(fr_thermal_gpu_source_state(view->gpu,&source,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
    view=dc_level_view_create_bonfire(directory,7,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&slot));
    fr_reactive_record_t restored;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,cell,&restored,&found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&burned,&restored,sizeof(burned)),0);
    ASSERT_TRUE(fr_thermal_gpu_source_state(view->gpu,&restored_source,err,sizeof(err)));
    ASSERT_EQ(memcmp(&source,&restored_source,sizeof(source)),0);
    uint32_t wet_slot;ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){5,1},&wet_slot));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,wet_slot,cell,&original,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(original.state.bound_water_fp>0);
    ASSERT_TRUE(dc_level_view_heat_probe(view,348,208,true));
    for(uint32_t i=0;i<360;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_heat_probe(view,348,208,false));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,wet_slot,cell,&restored,&found,err,sizeof(err))&&found);
    ASSERT_EQ(original.state.fuel_mass_fp,restored.state.fuel_mass_fp);
    ASSERT_EQ(restored.state.flags&FR_REACTIVE_FLAMING,0);
    ASSERT_TRUE(dc_level_view_paint(view,156,202,3,DC_MATERIAL_WATER,err,sizeof(err)));
    for(uint32_t i=0;i<360;i++) {
        ASSERT_TRUE(dc_level_view_paint(view,156,207,0,DC_MATERIAL_WATER,err,sizeof(err)));
        ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    }
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,cell,&restored,&found,err,sizeof(err))&&found);
    dc_chunk_t *doused=calloc(1,sizeof(*doused));ASSERT_TRUE(doused);
    ASSERT_TRUE(dc_gpu_download_chunk(view->gpu,slot,doused,err,sizeof(err)));
    ASSERT_TRUE(doused->cells[cell-64u].fluid_mass>0);
    printf("Doused wood: bound water %u surface water %u flags %u fuel %u\n",restored.state.bound_water_fp,
        doused->cells[cell-64u].fluid_mass,restored.state.flags,restored.state.fuel_mass_fp);free(doused);
    ASSERT_TRUE(restored.state.bound_water_fp>0);ASSERT_EQ(restored.state.flags&FR_REACTIVE_FLAMING,0);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
int main(void) {RUN(test_bonfire_is_a_playable_finite_terrain_scene);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
