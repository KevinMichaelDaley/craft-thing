#define _POSIX_C_SOURCE 200809L
#define main smoke_box_fixture_main
#include "../vulkan/reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include <sys/stat.h>
#include "../../src/app/level_internal.h"
#include "ferrum/fire/gas_gpu.h"

static bool smoke_budget(dc_level_view_t *view,double species[5],double *energy,double pigment[3],double *work,double *height,
    char *err,uint32_t cap) {
    fr_gas_state_t *states=malloc(4096*sizeof(*states));double *external=malloc(4096*sizeof(*external));if(!states||!external) {free(states);free(external);return false;}
    bool okay=true;memset(species,0,5*sizeof(*species));memset(pigment,0,3*sizeof(*pigment));*energy=0;*work=0;*height=0;
    for(uint32_t slot=0;okay&&slot<view->table.capacity;slot++) {
        okay=fr_gas_gpu_read_chunk(view->gpu,slot,states,4096,err,cap)&&fr_gas_gpu_read_work_chunk(view->gpu,slot,external,4096,err,cap);
        for(uint32_t i=0;okay&&i<4096;i++) {
            for(uint32_t k=0;k<5;k++) species[k]+=states[i].species[k];
            for(uint32_t k=0;k<3;k++) pigment[k]+=states[i].pigment[k];
            *energy+=states[i].energy;*work+=external[i];
            *height+=states[i].pigment[0]*(view->table.slots[slot].coord.y*64+(int64_t)(i/64u));
        }
    }
    free(states);free(external);return okay;
}
static void test_native_placement_box_has_finite_sources_real_controls_rising_smoke_and_stable_pause(void) {
    char err[256]={0},directory[]="build/smoke_box_native_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create_smoke_box(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_setup_smoke_box(view,err,sizeof(err)));
    uint32_t *first=malloc(480u*270u*4u),*second=malloc(480u*270u*4u);ASSERT_TRUE(first&&second);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_gpu_readback(view->gpu,first,480u*270u,err,sizeof(err)));
    fr_gas_gpu_clock_t before,after;ASSERT_TRUE(fr_gas_gpu_clock(view->gpu,&before)&&before.physical_steps==0u);
    for(uint32_t i=0;i<4;i++) ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(view->gpu,second,480u*270u,err,sizeof(err)));ASSERT_EQ(memcmp(first,second,480u*270u*4u),0);
    ASSERT_TRUE(fr_gas_gpu_clock(view->gpu,&after));ASSERT_EQ(memcmp(&before,&after,sizeof(before)),0);
    double stock[5],pigment[3],energy,work,height,moved[5],moved_pigment[3],moved_energy,moved_work,moved_height;
    ASSERT_TRUE(smoke_budget(view,stock,&energy,pigment,&work,&height,err,sizeof(err)));
    ASSERT_TRUE(stock[2]>0&&stock[2]<4&&pigment[0]>0&&pigment[2]>0);
    ASSERT_TRUE(dc_level_view_smoke_valve(view,0,err,sizeof(err))&&dc_level_view_smoke_valve(view,1,err,sizeof(err))&&dc_level_view_smoke_valve(view,2,err,sizeof(err)));
    for(uint32_t i=0;i<30;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_clock(view->gpu,&after));ASSERT_EQ(after.physical_steps,30u);ASSERT_TRUE(fabs(after.advanced_seconds-.5)<1e-7);
    ASSERT_TRUE(smoke_budget(view,moved,&moved_energy,moved_pigment,&moved_work,&moved_height,err,sizeof(err)));
    for(uint32_t k=0;k<5;k++) printf("Enclosure species%u stock%.12g drift%.12g relative%.12g\n",k,stock[k],moved[k]-stock[k],stock[k]>0?(moved[k]-stock[k])/stock[k]:0.);
    printf("Enclosure energy%.12g ledger drift%.12g relative%.12g\n",energy,(moved_energy-energy)-(moved_work-work),((moved_energy-energy)-(moved_work-work))/energy);
    for(uint32_t k=0;k<5;k++) {
        if(stock[k]==0.) ASSERT_EQ(moved[k],0.);
        else ASSERT_TRUE(fabs(moved[k]-stock[k])<fmax(1e-9,stock[k]*1e-5));
    }
    for(uint32_t k=0;k<3;k++) ASSERT_TRUE(fabs(moved_pigment[k]-pigment[k])<fmax(1e-9,pigment[k]*1e-5));
    ASSERT_TRUE(fabs((moved_energy-energy)-(moved_work-work))<energy*1e-5);
    ASSERT_TRUE(moved_height<height-.01);
    ASSERT_TRUE(dc_level_view_smoke_overlay(view,1)&&dc_level_view_tick(view,err,sizeof(err))&&dc_gpu_readback(view->gpu,second,480u*270u,err,sizeof(err)));
    ASSERT_TRUE(memcmp(first,second,480u*270u*4u)!=0);mkdir("build/screenshots",0777);
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormatFrom(second,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);ASSERT_TRUE(image);
    ASSERT_EQ(SDL_SaveBMP(image,"build/screenshots/smoke_box_oxygen.bmp"),0);SDL_FreeSurface(image);
    ASSERT_TRUE(dc_level_view_smoke_overlay(view,0)&&dc_level_view_tick(view,err,sizeof(err))&&dc_gpu_readback(view->gpu,second,480u*270u,err,sizeof(err)));
    image=SDL_CreateRGBSurfaceWithFormatFrom(second,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);ASSERT_TRUE(image);
    ASSERT_EQ(SDL_SaveBMP(image,"build/screenshots/smoke_box_color.bmp"),0);SDL_FreeSurface(image);
    free(first);free(second);ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
static void test_native_saved_smoke_box_restores_stock_and_valves_without_refilling(void) {
    char err[256]={0},directory[]="build/smoke_box_reload_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create_smoke_box(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_smoke_valve(view,0,err,sizeof(err)));
    for(uint32_t i=0;i<2u;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    uint32_t count=view->table.capacity;
    fr_gas_gpu_clock_t saved_clock,restored_clock;ASSERT_TRUE(fr_gas_gpu_clock(view->gpu,&saved_clock));
    fr_gas_checkpoint_chunk_t *saved=calloc(count,sizeof(*saved)),*restored=malloc(sizeof(*restored));
    dc_chunk_coord_t *coords=malloc(count*sizeof(*coords));ASSERT_TRUE(saved&&restored&&coords);
    for(uint32_t i=0;i<count;i++) {coords[i]=view->table.slots[i].coord;ASSERT_TRUE(fr_gas_gpu_export_checkpoint(view->gpu,i,saved+i,err,sizeof(err)));}
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
    view=dc_level_view_create_smoke_box(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(fr_gas_gpu_clock(view->gpu,&restored_clock));ASSERT_EQ(memcmp(&saved_clock,&restored_clock,sizeof(saved_clock)),0);
    ASSERT_EQ(view->smoke_closed,6u);
    for(uint32_t i=0;i<count;i++) {
        uint32_t slot;ASSERT_TRUE(dc_chunk_table_find(&view->table,coords[i],&slot));
        ASSERT_TRUE(fr_gas_gpu_export_checkpoint(view->gpu,slot,restored,err,sizeof(err)));
        ASSERT_EQ(memcmp(saved+i,restored,sizeof(*restored)),0);
    }
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_level_view_destroy(view,err,sizeof(err)));
    free(saved);free(restored);free(coords);PASS();
}
static uint64_t smoke_file_hash(const char *path) {
    FILE *file=fopen(path,"rb");if(!file) return 0;
    uint64_t hash=UINT64_C(14695981039346656037);uint8_t bytes[4096];size_t count;
    while((count=fread(bytes,1,sizeof(bytes),file))!=0u) hash=dc_color_hash(hash,bytes,count);
    bool okay=!ferror(file)&&fclose(file)==0;return okay?hash:0;
}
static void test_native_smoke_streaming_restores_sleeping_stock_and_initializes_new_slots(void) {
    char err[256]={0},directory[]="build/smoke_box_stream_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create_smoke_box(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_smoke_valve(view,0,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    fr_gas_checkpoint_chunk_t *before=malloc(sizeof(*before)),*out=malloc(sizeof(*out));ASSERT_TRUE(before&&out);
    uint32_t slot;ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){1,0},&slot));
    ASSERT_TRUE(fr_gas_gpu_export_checkpoint(view->gpu,slot,before,err,sizeof(err)));
    for(uint32_t i=0;i<20u;i++) ASSERT_TRUE(dc_level_view_move(view,1,0));
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    ASSERT_TRUE(!dc_chunk_table_find(&view->table,(dc_chunk_coord_t){1,0},&slot));
    double ambient_mass=0.;
    for(uint32_t i=0;i<view->table.capacity;i++) {
        ASSERT_TRUE(fr_gas_gpu_export_checkpoint(view->gpu,i,out,err,sizeof(err)));
        for(uint32_t cell=0;cell<4096u;cell++) {
            ASSERT_EQ(out->cells[cell][2],0.f);ASSERT_EQ(out->cells[cell][8],0.f);ASSERT_EQ(out->cells[cell][10],0.f);ASSERT_EQ(out->gates[cell],0u);
            ambient_mass+=out->cells[cell][0]+out->cells[cell][1];
        }
    }
    ASSERT_TRUE(ambient_mass>10.);
    for(uint32_t i=0;i<20u;i++) ASSERT_TRUE(dc_level_view_move(view,-1,0));
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err))&&dc_chunk_table_find(&view->table,(dc_chunk_coord_t){1,0},&slot));
    ASSERT_TRUE(fr_gas_gpu_export_checkpoint(view->gpu,slot,out,err,sizeof(err)));ASSERT_EQ(memcmp(before,out,sizeof(*before)),0);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));free(before);free(out);PASS();
}
static void check_missing_stream_gas(bool authoritative) {
    char err[256]={0},directory[]="build/smoke_box_missing_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create_smoke_box(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
    dc_streamer_t *stream=dc_stream_create(directory,314,4);ASSERT_TRUE(stream);
    ASSERT_TRUE(dc_stream_request_load(stream,(dc_chunk_coord_t){1,0},1));dc_stream_result_t result={0};
    uint64_t deadline=SDL_GetTicks64()+10000u;
    while(!dc_stream_poll(stream,&result)&&SDL_GetTicks64()<deadline) SDL_Delay(1);
    ASSERT_EQ(result.kind,DC_STREAM_LOADED);ASSERT_TRUE(result.chunk&&result.gas);
    fr_gas_checkpoint_chunk_t *expected=result.gas;
    result.gas=NULL;
    ASSERT_TRUE(dc_stream_request_save_thermal(stream,result.chunk,result.colors,result.reactive,result.water_excess,2));
    dc_stream_result_release(&result);deadline=SDL_GetTicks64()+10000u;
    while(!dc_stream_poll(stream,&result)&&SDL_GetTicks64()<deadline) SDL_Delay(1);
    ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);dc_stream_destroy(stream);
    char chunk_path[512],body_path[512];snprintf(chunk_path,sizeof(chunk_path),"%s/chunk_%016llx_%016llx.bin",directory,1ull,0ull);
    snprintf(body_path,sizeof(body_path),"%s/rigid_bodies.bin",directory);
    uint64_t chunk_hash=smoke_file_hash(chunk_path),body_hash=smoke_file_hash(body_path);ASSERT_TRUE(chunk_hash&&body_hash);
    view=dc_level_view_create_smoke_box(directory,314,err,sizeof(err));
    if(authoritative) {
        ASSERT_TRUE(!view);
        ASSERT_TRUE(strstr(err,"missing an authoritative gas chunk"));
    } else {
        fr_gas_checkpoint_chunk_t *restored=malloc(sizeof(*restored));
        uint32_t slot;
        ASSERT_TRUE(view&&restored&&dc_chunk_table_find(&view->table,(dc_chunk_coord_t){1,0},&slot));
        ASSERT_TRUE(fr_gas_gpu_export_checkpoint(view->gpu,slot,restored,err,sizeof(err)));
        ASSERT_EQ(memcmp(expected,restored,sizeof(*expected)),0);
        free(restored);
        dc_level_view_discard(view);
    }
    free(expected);
    ASSERT_EQ(smoke_file_hash(chunk_path),chunk_hash);ASSERT_EQ(smoke_file_hash(body_path),body_hash);PASS();
}
static void test_missing_superseded_gas_recovers_archive(void) {check_missing_stream_gas(false);}
static void test_native_incomplete_smoke_admission_fails_without_overwriting_saved_world(void) {check_missing_stream_gas(true);}
static void test_native_air_brush_opens_a_real_roof_vent_for_rising_smoke(void) {
    enum { ROOF_X = 328, ROOF_Y = 49, ROOF_RADIUS = 4,
           ROOF_CELL = ROOF_Y * DC_CHUNK_SIDE + ROOF_X % DC_CHUNK_SIDE,
           SAMPLE_TICKS = 60, VENT_TICKS = 600 };
    const double minimum_exhaust = 1e-9;
    char err[256]={0},directory[]="build/smoke_box_carved_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create_smoke_box(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_smoke_valve(view,0,err,sizeof(err))&&dc_level_view_smoke_valve(view,1,err,sizeof(err)));
    for(uint32_t i=0;i<120u;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    uint32_t slot;fr_gas_checkpoint_chunk_t *snapshot=malloc(sizeof(*snapshot));ASSERT_TRUE(snapshot);
    ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){5,-2},&slot)&&fr_gas_gpu_export_checkpoint(view->gpu,slot,snapshot,err,sizeof(err)));
    ASSERT_EQ(snapshot->cells[ROOF_CELL][13], 0.f);
    ASSERT_EQ(snapshot->cells[ROOF_CELL][FR_GAS_OXYGEN], 0.f);
    fr_gas_gpu_audit_t before, after;
    ASSERT_TRUE(fr_gas_gpu_audit(view->gpu, &before, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view,ROOF_X,ROOF_Y,ROOF_RADIUS,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_export_checkpoint(view->gpu, slot, snapshot, err, sizeof(err)));
    ASSERT_EQ(snapshot->cells[ROOF_CELL][13], 1.f);
    ASSERT_TRUE(snapshot->cells[ROOF_CELL][FR_GAS_OXYGEN] > 0.f);
    ASSERT_TRUE(fr_gas_gpu_audit(view->gpu, &after, err, sizeof(err)));
    for (uint32_t species = 0; species < FR_GAS_SPECIES; ++species) {
        const uint32_t metric = FR_GAS_AUDIT_SPECIES + species;
        const float stock = before.metrics[metric];
        ASSERT_TRUE(fabsf(after.metrics[metric] - stock) <= fmaxf(1e-9f, stock * 1e-5f));
    }
    double escaped=0.;
    uint32_t vent_ticks=0;
    while (vent_ticks < VENT_TICKS && escaped <= minimum_exhaust) {
        for (uint32_t i=0; i<SAMPLE_TICKS; ++i)
            ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)) && dc_level_view_tick(view,err,sizeof(err)));
        vent_ticks += SAMPLE_TICKS;
        escaped = 0.;
        for (int64_t x=4; x<=5; ++x) {
            ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){x,-2},&slot) &&
                        fr_gas_gpu_export_checkpoint(view->gpu,slot,snapshot,err,sizeof(err)));
            for (uint32_t row=24; row<47u; ++row) for (uint32_t column=0; column<DC_CHUNK_SIDE; ++column) {
                int64_t world_x = x * DC_CHUNK_SIDE + column;
                if (world_x >= ROOF_X - 16 && world_x < ROOF_X + 16)
                    escaped += snapshot->cells[row*DC_CHUNK_SIDE+column][FR_GAS_EXHAUST];
            }
        }
    }
    printf("Air-brush roof vent escaped exhaust %.9g after %u ticks\n", escaped, vent_ticks);
    ASSERT_TRUE(escaped > minimum_exhaust);
    uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels&&dc_level_view_pixels(view,pixels,480u*270u,err,sizeof(err)));
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);ASSERT_TRUE(image);
    ASSERT_EQ(SDL_SaveBMP(image,"build/screenshots/smoke_box_carved_vent.bmp"),0);SDL_FreeSurface(image);free(pixels);free(snapshot);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--restore-gas")) {
        RUN(test_missing_superseded_gas_recovers_archive);
        RUN(test_native_incomplete_smoke_admission_fails_without_overwriting_saved_world);
        return g_fail!=0;
    }
    setvbuf(stdout,NULL,_IOLBF,0);RUN(test_native_placement_box_has_finite_sources_real_controls_rising_smoke_and_stable_pause);
    RUN(test_native_saved_smoke_box_restores_stock_and_valves_without_refilling);
    RUN(test_missing_superseded_gas_recovers_archive);
    RUN(test_native_incomplete_smoke_admission_fails_without_overwriting_saved_world);
    RUN(test_native_smoke_streaming_restores_sleeping_stock_and_initializes_new_slots);
    RUN(test_native_air_brush_opens_a_real_roof_vent_for_rising_smoke);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
