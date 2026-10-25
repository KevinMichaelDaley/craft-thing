#define _POSIX_C_SOURCE 200809L
#define main thermal_bench_fixture_main
#include "../vulkan/reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include "../../src/app/level.c"
#include "../../src/app/session.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include <sys/stat.h>

static bool bench_capture(uint32_t *pixels,const char *path) {
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);
    bool okay=surface&&SDL_SaveBMP(surface,path)==0;if(surface) SDL_FreeSurface(surface);return okay;
}
/* Cold reference snapshots only. Runtime neither reads nor constructs these
 * cell/parcel lists; its transfer kernels are the subject of this check. */
static bool bench_budget(dc_level_view_t *view,uint64_t *water,double *energy,char *err,uint32_t cap) {
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));if(!chunk) return false;
    uint64_t mass=0;double total=0;bool okay=true;
    const double water_cp=fr_reactive_profile(DC_MATERIAL_WATER)->heat_capacity;
    for(int64_t y=0;okay&&y<2;y++) for(int64_t x=1;okay&&x<3;x++) {
        uint32_t slot=0;fr_reactive_chunk_t channel={0};float excess[4096];
        chunk->coord=(dc_chunk_coord_t){x,y};
        okay=dc_chunk_table_find(&view->table,(dc_chunk_coord_t){x,y},&slot)&&
            dc_gpu_download_chunk(view->gpu,slot,chunk,err,cap)&&
            fr_gpu_reactive_export_chunk(view->gpu,slot,chunk,&channel,err,cap)&&
            fr_thermal_gpu_read_water_chunk(view->gpu,slot,excess,4096,err,cap);
        if(okay) {
            for(uint32_t i=0;i<4096;i++) {
                if(chunk->cells[i].fluid_mass==0u&&excess[i]!=0.f) {
                    snprintf(err,cap,"Orphan heat chunk(%lld,%lld) cell%u excess%.9g",(long long)x,(long long)y,i,(double)excess[i]);
                    okay=false;break;
                }
                mass+=chunk->cells[i].fluid_mass;
                total+=(double)chunk->cells[i].fluid_mass/65536.*water_cp*FR_THERMAL_AMBIENT_K+excess[i];
            }
            for(uint32_t i=0;i<channel.count;i++) {
                mass+=channel.attachments[i].record.state.bound_water_fp;
                total+=channel.attachments[i].record.state.thermal_energy;
            }
        }
        fr_reactive_chunk_release(&channel);
    }
    free(chunk);if(okay) {*water=mass;*energy=total;}return okay;
}

static void test_interactive_bench_has_finite_sources_wet_control_probe_and_stable_pause(void) {
    char err[256]={0},directory[]="build/thermal_bench_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_setup_heat_bench(view,err,sizeof(err)));
    uint32_t dry_slot=0,wet_slot=0;ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&dry_slot));
    ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){5,1},&wet_slot));
    fr_reactive_record_t dry,wet;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,dry_slot,0,&dry,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,wet_slot,8,&wet,&found,err,sizeof(err))&&found);
    ASSERT_EQ(dry.state.material,DC_MATERIAL_WOOD);ASSERT_EQ(dry.state.bound_water_fp,0u);ASSERT_TRUE(wet.state.bound_water_fp>0u);
    ASSERT_TRUE(dc_level_view_heat_probe(view,140,192,true));
    for(uint32_t frame=0;frame<12;frame++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_heat_probe(view,140,192,false));
    fr_reactive_record_t heated;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,dry_slot,12,&heated,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(heated.state.thermal_energy>dry.state.thermal_energy);
    uint32_t *before=malloc(480u*270u*4u),*after=malloc(480u*270u*4u);ASSERT_TRUE(before&&after);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_level_view_pixels(view,before,480u*270u,err,sizeof(err)));
    mkdir("build/screenshots",0777);ASSERT_TRUE(bench_capture(before,"build/screenshots/thermal_bench_temperature.bmp"));
    for(uint32_t frame=0;frame<4;frame++) {
        ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_level_view_pixels(view,after,480u*270u,err,sizeof(err)));
        ASSERT_TRUE(memcmp(before,after,480u*270u*4u)==0);
    }
    ASSERT_TRUE(dc_level_view_heat_overlay(view,2)&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixels(view,after,480u*270u,err,sizeof(err)));
    ASSERT_TRUE(bench_capture(after,"build/screenshots/thermal_bench_wetness.bmp"));
    ASSERT_TRUE(dc_level_view_heat_overlay(view,0)&&dc_level_view_tick(view,err,sizeof(err)));
    free(before);free(after);ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
static void test_placement_water_pour_cools_and_wets_wood_with_closed_reference_budgets(void) {
    char err[256]={0},directory[]="build/thermal_pour_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err))&&dc_level_view_setup_heat_bench(view,err,sizeof(err)));
    uint32_t slot=0;ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&slot));
    ASSERT_TRUE(dc_level_view_heat_probe(view,140,192,true));
    for(uint32_t i=0;i<12;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_heat_probe(view,140,192,false));
    fr_reactive_record_t before,after;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,12,&before,&found,err,sizeof(err))&&found);
    double before_temperature=before.state.thermal_energy/fr_reactive_state_heat_capacity(&before.state);
    uint64_t initial_water=0,final_water=0;double initial_energy=0,final_energy=0;
    ASSERT_TRUE(bench_budget(view,&initial_water,&initial_energy,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view,140,188,3,DC_MATERIAL_WATER,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,12,&after,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(after.state.bound_water_fp>before.state.bound_water_fp);
    ASSERT_EQ(after.state.parcel_id_low,before.state.parcel_id_low);ASSERT_EQ(after.state.parcel_id_high,before.state.parcel_id_high);
    double after_temperature=after.state.thermal_energy/fr_reactive_state_heat_capacity(&after.state);
    ASSERT_TRUE(after_temperature<before_temperature-1e-4);
    ASSERT_TRUE(bench_budget(view,&final_water,&final_energy,err,sizeof(err)));
    uint32_t cells=0;for(int y=-3;y<=3;y++) for(int x=-3;x<=3;x++) cells+=x*x+y*y<=9;
    ASSERT_EQ(final_water-initial_water,(uint64_t)cells*65536u);
    double injected=cells*(double)fr_reactive_profile(DC_MATERIAL_WATER)->heat_capacity*FR_THERMAL_AMBIENT_K;
    printf("Placement pour: wood %.6fK -> %.6fK, bound water%u, mass%llu/%u, energy error%.9f\n",
        before_temperature,after_temperature,after.state.bound_water_fp,(unsigned long long)(final_water-initial_water),cells*65536u,
        final_energy-initial_energy-injected);
    ASSERT_TRUE(fabs(final_energy-initial_energy-injected)<.02);
    for(uint32_t i=0;i<60;i++) {
        ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
        printf("Pour reference step%u\n",i+2u);
        ASSERT_TRUE(bench_budget(view,&final_water,&final_energy,err,sizeof(err)));
    }
    uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels);
    ASSERT_TRUE(dc_level_view_heat_overlay(view,2)&&dc_level_view_tick(view,err,sizeof(err))&&dc_level_view_pixels(view,pixels,480u*270u,err,sizeof(err)));
    mkdir("build/screenshots",0777);ASSERT_TRUE(bench_capture(pixels,"build/screenshots/thermal_bench_water_pour.bmp"));
    free(pixels);ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
static void test_reopened_bench_retains_warm_parcels_water_source_and_custom_colors(void) {
    char err[256]={0},directory[]="build/thermal_resume_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_setup_heat_bench(view,err,sizeof(err))&&dc_level_view_heat_probe(view,140,192,true));
    for(uint32_t i=0;i<12;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_heat_probe(view,140,192,false)&&dc_level_view_paint(view,140,188,3,DC_MATERIAL_WATER,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    uint32_t slot=0,water_slot=0;bool found=false;
    ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&slot)&&dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,0},&water_slot));
    fr_reactive_record_t before,after;fr_thermal_probe_t source,fresh_source;float heat[4096],fresh_heat[4096];
    dc_chunk_colors_t *colors=malloc(sizeof(*colors)),*fresh_colors=malloc(sizeof(*fresh_colors));ASSERT_TRUE(colors&&fresh_colors);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,12,&before,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_thermal_gpu_source_state(view->gpu,&source,err,sizeof(err))&&source.excess_energy<11370.f);
    ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(view->gpu,water_slot,heat,4096,err,sizeof(err))&&fr_renderer_download_chunk_colors(view->gpu,slot,colors,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));view=dc_level_view_create(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_restored(view)&&dc_level_view_setup_heat_bench(view,err,sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&slot)&&dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,0},&water_slot));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,12,&after,&found,err,sizeof(err))&&found&&memcmp(&before,&after,sizeof(before))==0);
    ASSERT_TRUE(fr_thermal_gpu_source_state(view->gpu,&fresh_source,err,sizeof(err))&&memcmp(&source,&fresh_source,sizeof(source))==0);
    ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(view->gpu,water_slot,fresh_heat,4096,err,sizeof(err))&&memcmp(heat,fresh_heat,sizeof(heat))==0);
    ASSERT_TRUE(fr_renderer_download_chunk_colors(view->gpu,slot,fresh_colors,err,sizeof(err))&&memcmp(colors,fresh_colors,sizeof(*colors))==0);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_level_view_destroy(view,err,sizeof(err)));
    free(colors);free(fresh_colors);PASS();
}
static void test_explicit_new_run_resets_source_and_wetness_without_reusing_old_world(void) {
    char err[256]={0},directory[]="build/thermal_restart_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_setup_heat_bench(view,err,sizeof(err))&&dc_level_view_heat_probe(view,140,192,true));
    for(uint32_t i=0;i<3;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    fr_thermal_probe_t spent,fresh;ASSERT_TRUE(fr_thermal_gpu_source_state(view->gpu,&spent,err,sizeof(err))&&spent.excess_energy<11370.f);
    ASSERT_TRUE(dc_app_restart_view(&view,directory,2718,1,err,sizeof(err))&&dc_level_view_setup_heat_bench(view,err,sizeof(err)));
    ASSERT_TRUE(!dc_level_view_restored(view)&&fr_thermal_gpu_source_state(view->gpu,&fresh,err,sizeof(err)));
    ASSERT_EQ(fresh.excess_energy,11370.f);ASSERT_EQ(fresh.capacity,200.f);
    uint32_t slot=0;bool found=false;fr_reactive_record_t wood;
    ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&slot)&&fr_gpu_reactive_read_cell(view->gpu,slot,12,&wood,&found,err,sizeof(err))&&found);
    ASSERT_EQ(wood.state.bound_water_fp,0u);
    ASSERT_TRUE(fabs(wood.state.thermal_energy/fr_reactive_state_heat_capacity(&wood.state)-FR_THERMAL_AMBIENT_K)<1e-3);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
static void test_camera_unload_reload_preserves_liquid_heat_without_recharging_source(void) {
    char err[256]={0},directory[]="build/thermal_camera_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err))&&dc_level_view_setup_heat_bench(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view,140,188,0,DC_MATERIAL_WATER,err,sizeof(err)));
    uint32_t slot=0;ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,0},&slot));
    const uint32_t cell=60u*64u+12u;
    ASSERT_TRUE(fr_thermal_gpu_seed_water(view->gpu,slot,cell,12.5f,err,sizeof(err)));
    float before[4096],after[4096];fr_thermal_probe_t source_before,source_after;
    ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(view->gpu,slot,before,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_source_state(view->gpu,&source_before,err,sizeof(err)));
    for(uint32_t i=0;i<20;i++) ASSERT_TRUE(dc_level_view_move(view,1,0));
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    ASSERT_TRUE(!dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,0},&slot));
    for(uint32_t i=0;i<20;i++) ASSERT_TRUE(dc_level_view_move(view,-1,0));
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,0},&slot));
    ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(view->gpu,slot,after,4096,err,sizeof(err)));
    ASSERT_TRUE(memcmp(before,after,sizeof(before))==0);
    ASSERT_TRUE(fr_thermal_gpu_source_state(view->gpu,&source_after,err,sizeof(err)));
    ASSERT_TRUE(memcmp(&source_before,&source_after,sizeof(source_before))==0);
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_download_chunk(view->gpu,slot,chunk,err,sizeof(err)));ASSERT_EQ(chunk->cells[cell].fluid_mass,65536u);free(chunk);
    ASSERT_TRUE(dc_level_view_heat_probe(view,140,188,false)&&dc_level_view_heat_overlay(view,1)&&dc_level_view_tick(view,err,sizeof(err)));
    uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels&&dc_level_view_pixels(view,pixels,480u*270u,err,sizeof(err)));
    mkdir("build/screenshots",0777);ASSERT_TRUE(bench_capture(pixels,"build/screenshots/thermal_bench_stream_resume.bmp"));free(pixels);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IOLBF,0);
    if(argc==2&&strcmp(argv[1],"--pour")==0) {
        RUN(test_placement_water_pour_cools_and_wets_wood_with_closed_reference_budgets);
        printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
    }
    if(argc==2&&strcmp(argv[1],"--stream")==0) {
        RUN(test_camera_unload_reload_preserves_liquid_heat_without_recharging_source);
        printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
    }
    if(argc!=1) {fprintf(stderr,"Usage: %s [--pour|--stream]\n",argv[0]);return 2;}
    RUN(test_interactive_bench_has_finite_sources_wet_control_probe_and_stable_pause);
    RUN(test_placement_water_pour_cools_and_wets_wood_with_closed_reference_budgets);
    RUN(test_reopened_bench_retains_warm_parcels_water_source_and_custom_colors);
    RUN(test_explicit_new_run_resets_source_and_wetness_without_reusing_old_world);
    RUN(test_camera_unload_reload_preserves_liquid_heat_without_recharging_source);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
