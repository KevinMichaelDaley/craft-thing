#define main combustion_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include <sys/stat.h>
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/combustion.h"
#include "dungeoncraft/combustion_parameters.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/structural/gpu.h"
#include "../../src/vulkan/gas_gpu_state.h"

static void terrain_combustion(bool practical,bool char_stage) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20u*64u+20u;
    chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,100,&wood,err,sizeof(err)));
    if(char_stage) {
        wood.char_mass_fp=wood.fuel_mass_fp;wood.fuel_mass_fp=0;
        wood.thermal_energy=(float)(fr_reactive_state_heat_capacity(&wood)*800.);
    }
    wood.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err)));
    if(practical) ASSERT_TRUE(fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_enable(g,err,sizeof(err)));
    if(practical) ASSERT_TRUE(fr_gas_gpu_step(g,.001f,err,sizeof(err)));
    fr_gas_state_t *before=calloc(4096,sizeof(*before)),*after=calloc(4096,sizeof(*after));ASSERT_TRUE(before&&after);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,before,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_step(g,.05f,err,sizeof(err)));
    if(practical) ASSERT_TRUE(g->gas->cache_valid);
    fr_reactive_record_t result;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&result,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(result.state.fuel_mass_fp+result.state.char_mass_fp<wood.fuel_mass_fp+wood.char_mass_fp);
    ASSERT_TRUE(fr_reactive_state_valid(&result.state,err,sizeof(err)));
    fr_combustion_emission_t *emission=calloc(4096,sizeof(*emission)),*zero_step=calloc(4096,sizeof(*zero_step));ASSERT_TRUE(emission&&zero_step);
    ASSERT_TRUE(fr_combustion_gpu_read_emission_chunk(g,0,emission,4096,err,sizeof(err)));
    double surface_power=0;for(uint32_t i=0;i<4096;i++) if(emission[i].sources&1u) surface_power+=emission[i].power;
    ASSERT_TRUE(surface_power>0);
    ASSERT_TRUE(fr_combustion_gpu_step(g,0,err,sizeof(err))&&fr_combustion_gpu_read_emission_chunk(g,0,zero_step,4096,err,sizeof(err)));
    ASSERT_EQ(memcmp(emission,zero_step,4096*sizeof(*emission)),0);free(emission);free(zero_step);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));
    double oxygen=0,products=0,mass_before=fr_reactive_state_mass_fp(&wood)/65536.,
        mass_after=fr_reactive_state_mass_fp(&result.state)/65536.;
    for(uint32_t i=0;i<4096;i++) {
        oxygen+=before[i].species[FR_GAS_OXYGEN]-after[i].species[FR_GAS_OXYGEN];
        products+=after[i].species[FR_GAS_EXHAUST]+after[i].species[FR_GAS_SOOT];
        for(uint32_t species=0;species<FR_GAS_SPECIES;species++) {
            mass_before+=before[i].species[species];mass_after+=after[i].species[species];
        }
    }
    ASSERT_TRUE(oxygen>0&&products>oxygen);
    ASSERT_TRUE(fabs(mass_before-mass_after)<1e-7);
    uint32_t fuel_before_short_steps=result.state.fuel_mass_fp+result.state.char_mass_fp;
    for(uint32_t step=0;step<20;step++) ASSERT_TRUE(fr_combustion_gpu_step(g,.001f,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&result,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(result.state.fuel_mass_fp+result.state.char_mass_fp<fuel_before_short_steps&&result.state.char_mass_fp>0);
    if(char_stage) {
        result.state.thermal_energy=(float)(fr_reactive_state_heat_capacity(&result.state)*350.);
        result.state.thermal_energy_residual=result.state.thermal_energy_correction=0;
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&result.state,err,sizeof(err))&&fr_combustion_gpu_step(g,.001f,err,sizeof(err)));
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&result,&found,err,sizeof(err))&&found);
        ASSERT_TRUE(fr_thermal_gpu_overlay(g,0));
    }
    ASSERT_TRUE(present(g,err,sizeof(err)));
    if(char_stage) {
        uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels);
        ASSERT_TRUE(dc_gpu_readback(g,pixels,480u*270u,err,sizeof(err)));
        uint32_t color=pixels[20u*480u+20u];free(pixels);
        ASSERT_TRUE((color&255u)+((color>>8)&255u)+((color>>16)&255u)<100u);
    }
    fr_reactive_record_t paused;ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&paused,&found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&result,&paused,sizeof(result)),0);
    free(before);free(after);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_displayed_terrain_combustion_debits_real_fuel_and_oxygen(void) {terrain_combustion(false,false);}
static void test_sparse_owner_combustion_uses_the_live_gas_bank(void) {terrain_combustion(true,false);}
static void test_retained_char_can_finish_burning_on_the_gpu(void) {terrain_combustion(true,true);}
static void test_spent_terrain_becomes_loose_saved_ash_without_losing_mass(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20u*64u+20u;
    chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    uint32_t protected_at=at+2u;chunk->cells[protected_at]=chunk->cells[at];
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t spent;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,2000,0,300,70000,&spent,err,sizeof(err)));
    spent.inert_mass_fp=spent.fuel_mass_fp;spent.fuel_mass_fp=0;spent.flags=FR_REACTIVE_SPENT;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&spent,err,sizeof(err)));
    fr_reactive_state_t protected=spent;protected.coating_oil_fuel_fp=200;
    protected.parcel_id_low=protected.origin_id_low=70001;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,protected_at,&protected,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_enable(g,err,sizeof(err)));
    dc_gpu_structural_data(g)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD]=0;
    ASSERT_TRUE(fr_combustion_gpu_step(g,.01f,err,sizeof(err)));
    ASSERT_EQ(dc_gpu_structural_data(g)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD],1);
    fr_reactive_record_t record;bool found=true;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,protected_at,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(record.state.coating_oil_fuel_fp,200);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&record,&found,err,sizeof(err))&&!found);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_EQ(chunk->cells[at].material,DC_MATERIAL_ASH);ASSERT_EQ(chunk->cells[at].flags,0);
    ASSERT_EQ(chunk->particles[at].mass_fp,2000);ASSERT_EQ(chunk->particles[at].material,DC_MATERIAL_ASH);
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,at,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(record.state.material,DC_MATERIAL_ASH);ASSERT_EQ(fr_reactive_state_mass_fp(&record.state),2000);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true)&&dc_gpu_set_tick_seconds(g,1.f/60.f));
    for(uint32_t i=0;i<4;i++) ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err))&&present(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    uint32_t particle=UINT32_MAX;
    for(uint32_t i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) if(chunk->particles[i].mass_fp) {
        ASSERT_EQ(chunk->particles[i].mass_fp,2000);ASSERT_EQ(particle,UINT32_MAX);particle=i;
    }
    ASSERT_TRUE(particle!=UINT32_MAX&&chunk->particles[particle].y_fp>20*65536+32768);
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,particle,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(fr_reactive_state_mass_fp(&record.state),2000);
    fr_reactive_chunk_t channel={0};ASSERT_TRUE(fr_gpu_reactive_export_chunk(g,0,chunk,&channel,err,sizeof(err)));
    printf("Ash save channel: %u records particle %u\n",channel.count,particle);
    ASSERT_EQ(channel.count,2);fr_reactive_chunk_release(&channel);
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/combustion_loose_ash.bin",err,sizeof(err)));
    dc_gpu_destroy(g);g=NULL;ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g,"build/combustion_loose_ash.bin",err,sizeof(err)));
    fr_gpu_reactive_stats_t stats;ASSERT_TRUE(fr_gpu_reactive_read_stats(g,&stats,err,sizeof(err)));
    printf("Ash restore: %u live entries, %u pending chunks\n",stats.live,g->checkpoint_chunk_count);
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,particle,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(record.state.material,DC_MATERIAL_ASH);ASSERT_EQ(fr_reactive_state_mass_fp(&record.state),2000);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_flame_emission_requires_local_transported_hot_fuel_and_oxygen(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_overlay(g,0));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*output=calloc(4096,sizeof(*output));
    double *volumes=malloc(4096*sizeof(*volumes));uint32_t *pixels=malloc(480u*270u*4u);
    ASSERT_TRUE(input&&output&&volumes&&pixels);for(uint32_t i=0;i<4096;i++) volumes[i]=1;
    uint32_t at=20u*64u+20u;
    for(uint32_t control=0;control<5;control++) {
        fr_gas_state_t source={0};source.species[FR_GAS_OXYGEN]=control==3?0:.00028;
        source.species[FR_GAS_NITROGEN]=.00072;source.species[control==1?FR_GAS_EXHAUST:FR_GAS_SOOT]=.0002;
        for(uint32_t i=0;i<FR_GAS_SPECIES;i++) source.energy+=source.species[i]*model.heat_capacity_cv[i]*(control==2?300:control==4?480:1100);
        input[at]=source;fr_gas_state_t expected;double reacted=0;
        ASSERT_TRUE(fr_combustion_gas_reference(&model,&source,1,.001,&expected,&reacted,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err)));
        if(control==1u) {
            ASSERT_TRUE(present(g,err,sizeof(err))&&dc_gpu_readback(g,pixels,480u*270u,err,sizeof(err)));
            ASSERT_TRUE((pixels[20u*480u+20u]&255u)<100u);
        }
        ASSERT_TRUE(fr_combustion_gpu_step(g,.001f,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,output,4096,err,sizeof(err)));
        ASSERT_TRUE(fabs(output[at].species[FR_GAS_SOOT]-expected.species[FR_GAS_SOOT])<1e-9);
        ASSERT_TRUE(fabs(output[at].energy-expected.energy)<1e-5);
        ASSERT_TRUE(present(g,err,sizeof(err))&&dc_gpu_readback(g,pixels,480u*270u,err,sizeof(err)));
        uint32_t red=pixels[20u*480u+20u]&255u;
        if(control==0) {ASSERT_TRUE(reacted>0&&red>180u);}
        else {ASSERT_EQ(reacted,0);ASSERT_TRUE(red<100u);}
    }
    free(input);free(output);free(volumes);free(pixels);free(chunk);dc_gpu_destroy(g);PASS();
}
/* Cold fixture/reference inspection only. Production never reads these lists. */
static void transported_flame(uint32_t scenario,double *above,double *light_x) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*output=calloc(4096,sizeof(*output));
    double *volumes=malloc(4096*sizeof(*volumes));uint32_t *pixels=malloc(480u*270u*4u);
    ASSERT_TRUE(input&&output&&volumes&&pixels);
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    for(uint32_t y=0;y<64;y++) for(uint32_t x=0;x<64;x++) {
        uint32_t i=y*64+x;bool roof=scenario>=2&&y==24&&x>=16&&x<48&&!(scenario==3&&x>=32&&x<36);
        volumes[i]=roof?0:1;if(roof) {chunk->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};continue;}
        fr_gas_state_t *s=input+i;s->species[FR_GAS_OXYGEN]=.00028;s->species[FR_GAS_NITROGEN]=.00092;
        if(x>=28&&x<31&&y>=26&&y<29) s->species[FR_GAS_SOOT]=.000005;
        double mass=0;
        for(uint32_t species=0;species<FR_GAS_SPECIES;species++) {
            mass+=s->species[species];s->energy+=s->species[species]*model.heat_capacity_cv[species]*1100.;
        }
        double vx=60,vy=scenario==0?0:-60;s->momentum[0]=mass*vx;s->momentum[1]=mass*vy;
        s->energy+=.5*model.kinetic_scale*mass*(vx*vx+vy*vy);
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,1100,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_overlay(g,0));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err)));
    for(uint32_t frame=0;frame<10;frame++) {
        ASSERT_TRUE(fr_combustion_gpu_step(g,.01f,err,sizeof(err))&&fr_gas_gpu_step(g,.01f,err,sizeof(err)));
        ASSERT_TRUE(present(g,err,sizeof(err)));
    }
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,output,4096,err,sizeof(err))&&dc_gpu_readback(g,pixels,480u*270u,err,sizeof(err)));
    double total=0,above_mass=0,weighted_x=0,light=0,light_weighted_x=0;
    for(uint32_t y=0;y<64;y++) for(uint32_t x=0;x<64;x++) {
        uint32_t i=y*64+x;double carbon=output[i].species[FR_GAS_SOOT];total+=carbon;weighted_x+=carbon*x;
        if(y<24) above_mass+=carbon;
        uint32_t pixel=pixels[y*480u+x],r=pixel&255u,green=(pixel>>8)&255u,b=(pixel>>16)&255u;
        if(r>100u&&r>green+30u&&r>2u*b) {
            fr_gas_thermo_t thermo;
            ASSERT_TRUE(fr_gas_thermodynamics(&model,output+i,volumes[i],&thermo,err,sizeof(err)));
            ASSERT_TRUE(carbon>0&&thermo.temperature_K>FR_COMBUSTION_AIRBORNE_IGNITION_K-1&&output[i].species[FR_GAS_OXYGEN]/thermo.mass>.1199);
            ASSERT_TRUE(chunk->cells[i].material==DC_MATERIAL_AIR);
            light+=r-100u;light_weighted_x+=(r-100u)*x;
        }
    }
    ASSERT_TRUE(total>0&&light>0);
    *above=above_mass/total;*light_x=light_weighted_x/light;
    printf("Transported flame case%u: carbon x%.4f emission x%.4f above roof%.6f\n",scenario,weighted_x/total,*light_x,*above);
    ASSERT_TRUE(weighted_x/total>29.5&&*light_x>29.5);
    mkdir("build/screenshots",0777);char path[128];snprintf(path,sizeof(path),"build/screenshots/transported_flame_%u.bmp",scenario);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);
    ASSERT_TRUE(surface&&SDL_SaveBMP(surface,path)==0);SDL_FreeSurface(surface);
    free(input);free(output);free(volumes);free(pixels);free(chunk);dc_gpu_destroy(g);
}
static void test_flame_moves_with_real_lateral_gas_transport(void) {
    double above=0,x=0;int previous=g_fail;transported_flame(0,&above,&x);if(g_fail!=previous) return;PASS();
}
static void test_roof_blocks_and_side_vent_routes_the_physical_flame(void) {
    char err[256]={0};double above[3]={0},x=0;
    for(uint32_t scenario=1;scenario<=3;scenario++) {
        int previous=g_fail;transported_flame(scenario,above+scenario-1,&x);if(g_fail!=previous) return;
    }
    ASSERT_TRUE(above[0]>.02&&above[1]<.001&&above[2]>.02);PASS();
}
#include "combustion_vent_cases.h"
#include "combustion_containment_cases.h"
#include "combustion_contact_cases.h"
int main(int argc,char **argv) {
    if(argc==2&&strcmp(argv[1],"--contact")==0) {
        RUN(test_hot_gas_heats_each_exposed_wood_face);
        printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
    }
    if(argc==2&&strcmp(argv[1],"--containment")==0) {
        RUN(test_stone_and_clearance_contain_while_a_fuel_bridge_spreads);
        printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
    }
    if(argc==2&&strcmp(argv[1],"--vent")==0) {
        RUN(test_oxygen_starvation_and_finite_vented_recovery);
        printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
    }
    RUN(test_displayed_terrain_combustion_debits_real_fuel_and_oxygen);
    RUN(test_sparse_owner_combustion_uses_the_live_gas_bank);
    RUN(test_retained_char_can_finish_burning_on_the_gpu);
    RUN(test_spent_terrain_becomes_loose_saved_ash_without_losing_mass);
    RUN(test_flame_emission_requires_local_transported_hot_fuel_and_oxygen);
    RUN(test_flame_moves_with_real_lateral_gas_transport);
    RUN(test_roof_blocks_and_side_vent_routes_the_physical_flame);
    RUN(test_oxygen_starvation_and_finite_vented_recovery);
    RUN(test_stone_and_clearance_contain_while_a_fuel_bridge_spreads);
    RUN(test_hot_gas_heats_each_exposed_wood_face);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
