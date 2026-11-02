#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "../../src/app/beaver_fire_demo.h"
#include "../../src/app/level_internal.h"
#include "../../src/app/player_session.h"
#include "../../src/app/view_config.h"
#include "../../src/vulkan/runtime_body_gpu.h"
#include "../../src/vulkan/reactive_storage_gpu.h"

#define DEMO_TRANSACTION UINT64_C(0x424f4e4600000001)
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,err);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
typedef struct {uint64_t fuel,char_mass;uint32_t flaming;} pile_state_t;
static bool pile_state(dc_gpu_t *gpu,pile_state_t *out,char *err,uint32_t cap) {
    fr_reactive_record_t *records=NULL;uint32_t count=0;uint64_t next;
    if(!fr_gpu_reactive_export_bodies(gpu,&records,&count,&next,err,cap)) return false;
    pile_state_t result={0};
    for(uint32_t n=0;n<count;n++) {
        bool wood=false;
        for(uint32_t slot=0;slot<dc_gpu_body_capacity(gpu);slot++) {
            dc_body_storage_owner_t *o=dc_gpu_body_owner(gpu,slot);
            if(o->state==DC_BODY_OWNER_LIVE&&o->runtime_id==records[n].owner.key[0]&&
               o->source_transaction.low==(uint32_t)DEMO_TRANSACTION&&o->source_transaction.high==(uint32_t)(DEMO_TRANSACTION>>32)) wood=true;
        }
        if(!wood) continue;
        result.fuel+=records[n].state.fuel_mass_fp;result.char_mass+=records[n].state.char_mass_fp;
        result.flaming+=(records[n].state.flags&FR_REACTIVE_FLAMING)!=0;
    }
    free(records);*out=result;return true;
}
static bool capture(dc_level_view_t *v,const char *name,char *err,uint32_t cap) {
    uint32_t *pixels=malloc((size_t)VIEW_WIDTH*VIEW_HEIGHT*4);
    bool okay=pixels&&dc_level_view_pixels(v,pixels,VIEW_WIDTH*VIEW_HEIGHT,err,cap);
    SDL_Surface *surface=okay?SDL_CreateRGBSurfaceWithFormatFrom(pixels,VIEW_WIDTH,VIEW_HEIGHT,32,VIEW_WIDTH*4,SDL_PIXELFORMAT_ABGR8888):NULL;
    char path[256];snprintf(path,sizeof(path),"build/agents/beaver-player/sustained/%s.bmp",name);
    okay=surface&&SDL_SaveBMP(surface,path)==0;
    if(surface) SDL_FreeSurface(surface);
    free(pixels);return okay;
}
static void test_actual_demo_starts_unlit_and_button_ray_burns_real_wood(void) {
    char dir[]="build/beaver_fire_test_XXXXXX",err[256]={0};ASSERT_TRUE(mkdtemp(dir));
    dc_beaver_fire_plan_t plan;ASSERT_TRUE(dc_beaver_fire_plan(VIEW_WIDTH,VIEW_HEIGHT,&plan));
    dc_ground_profile_t ground={.width=1,.soil_depth=1,.surface=&plan.ground};
    dc_level_view_t *v=dc_level_view_create_ground(dir,314,&ground,err,sizeof(err));ASSERT_TRUE(v);
    ASSERT_TRUE(dc_level_view_pan_pixels(v,0,-INITIAL_CHUNK_Y*64));ASSERT_TRUE(dc_level_view_wait_visible(v,120000,err,sizeof(err)));
    ASSERT_TRUE(dc_beaver_fire_setup(v,err,sizeof(err)));
    uint32_t logs=0,target=0;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(v->gpu);slot++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(v->gpu,slot);
        if(owner->state!=DC_BODY_OWNER_LIVE||owner->source_transaction.low!=(uint32_t)DEMO_TRANSACTION||
           owner->source_transaction.high!=(uint32_t)(DEMO_TRANSACTION>>32)) continue;
        fr_reactive_record_t parcel;bool found=false;
        ASSERT_TRUE(fr_gpu_reactive_read_body(v->gpu,owner->runtime_id,0,0,&parcel,&found,err,sizeof(err)));ASSERT_TRUE(found);
        ASSERT_EQ(parcel.state.flags&(FR_REACTIVE_FLAMING|FR_REACTIVE_SMOLDERING),0);
        ASSERT_TRUE(parcel.state.fuel_mass_fp>0);
        if(owner->source_object==101) target=owner->runtime_id;
        logs++;
    }
    ASSERT_EQ(logs,DC_BEAVER_FIRE_LOGS);ASSERT_TRUE(target);
    pile_state_t pile_before,pile_after;
    ASSERT_TRUE(pile_state(v->gpu,&pile_before,err,sizeof(err)));ASSERT_EQ(pile_before.flaming,0);ASSERT_EQ(pile_before.char_mass,0);
    ASSERT_TRUE(dc_level_view_tick(v,err,sizeof(err)));ASSERT_TRUE(capture(v,"demo-unlit",err,sizeof(err)));
    fr_reactive_record_t before,after;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(v->gpu,target,0,8,&before,&found,err,sizeof(err)));ASSERT_TRUE(found);
    SDL_Event e={.type=SDL_KEYDOWN};
    double idle_ms=0,move_ms=0,cast_ms=0,burn_ms=0;unsigned idle_n=0,move_n=0,cast_n=0,burn_n=0;
    for(unsigned n=0;n<330;n++) {
        if(n==12) {e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_D;ASSERT_TRUE(dc_players_event(v,&e));}
        if(n==24) {e.type=SDL_KEYUP;e.key.keysym.scancode=SDL_SCANCODE_D;ASSERT_TRUE(dc_players_event(v,&e));}
        if(n==25) {e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_Q;ASSERT_TRUE(dc_players_event(v,&e));}
        Uint64 start=SDL_GetPerformanceCounter();
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
        double ms=(SDL_GetPerformanceCounter()-start)*1000./SDL_GetPerformanceFrequency();
        if(n>=8&&n<12) {idle_ms+=ms;idle_n++;}
        if(n>=12&&n<24) {move_ms+=ms;move_n++;}
        if(n>=25&&n<77) {cast_ms+=ms;cast_n++;}
        if(n>=210) {burn_ms+=ms;burn_n++;}
        if(n==54) ASSERT_TRUE(capture(v,"demo-ray",err,sizeof(err)));
    }
    printf("Demo actual window milliseconds: idle%.3f move%.3f cast/ray%.3f burning%.3f\n",idle_ms/idle_n,move_ms/move_n,cast_ms/cast_n,burn_ms/burn_n);
    ASSERT_TRUE(capture(v,"demo-burning",err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(v->gpu,target,0,8,&after,&found,err,sizeof(err)));ASSERT_TRUE(found);
    printf("wood fuel before%u after%u, char%u flags%u heat%.9g shock%.9g ignition%.9g threshold%.9g\n",before.state.fuel_mass_fp,after.state.fuel_mass_fp,after.state.char_mass_fp,after.state.flags,
        fr_reactive_state_energy(&after.state),after.state.pending_shock_energy,after.state.ignition_integral,after.state.ignition_threshold);
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(v->gpu);slot++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(v->gpu,slot);
        if(owner->runtime_id!=target) continue;
        dc_gpu_body_record_t *record=dc_gpu_body_record(v->gpu,slot);
        printf("target body x%d y%d width%u height%u angle%.9g mass%.9g\n",record->body.x_fp/65536,record->body.y_fp/65536,record->body.width,record->body.height,record->angle,record->mass);
    }
    ASSERT_TRUE(pile_state(v->gpu,&pile_after,err,sizeof(err)));
    printf("pile fuel before%llu after%llu char%llu flaming%u\n",(unsigned long long)pile_before.fuel,
        (unsigned long long)pile_after.fuel,(unsigned long long)pile_after.char_mass,pile_after.flaming);
    ASSERT_TRUE(pile_after.fuel<pile_before.fuel);ASSERT_TRUE(pile_after.char_mass>0);ASSERT_TRUE(pile_after.flaming>0);
    fr_gas_gpu_audit_t gas;ASSERT_TRUE(fr_gas_gpu_audit(v->gpu,&gas,err,sizeof(err)));
    ASSERT_TRUE(gas.metrics[FR_GAS_AUDIT_SPECIES+FR_GAS_EXHAUST]>0);
    fr_player_t player;fr_beaver_body_t body;ASSERT_TRUE(dc_players_status(v,0,&player,&body));ASSERT_EQ(player.action,FR_PLAYER_IDLE);
    ASSERT_TRUE(dc_level_view_destroy(v,err,sizeof(err)));PASS();
}
int main(void) {RUN(test_actual_demo_starts_unlit_and_button_ray_burns_real_wood);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
