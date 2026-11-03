#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../../src/app/player_session.h"
#include "../../src/app/view_config.h"
#include "../player/runtime_visual_check.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,err);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static bool capture(dc_level_view_t *v,const char *path,char *err,uint32_t cap) {
    mkdir("build/agents",0777);mkdir("build/agents/beaver-player",0777);
    mkdir("build/agents/beaver-player/sustained",0777);
    uint32_t *pixels=malloc((size_t)VIEW_WIDTH*VIEW_HEIGHT*4);
    bool okay=pixels&&dc_level_view_pixels(v,pixels,VIEW_WIDTH*VIEW_HEIGHT,err,cap);
    SDL_Surface *surface=okay?SDL_CreateRGBSurfaceWithFormatFrom(pixels,VIEW_WIDTH,VIEW_HEIGHT,32,VIEW_WIDTH*4,SDL_PIXELFORMAT_ABGR8888):NULL;
    okay=surface&&SDL_SaveBMP(surface,path)==0;
    if(surface) SDL_FreeSurface(surface);
    free(pixels);return okay;
}
static void test_real_app_keyboard_drives_body_and_animation(void) {
    char directory[]="build/player_runtime_XXXXXX",err[256]={0};ASSERT_TRUE(mkdtemp(directory));
    uint32_t surface=96;dc_ground_profile_t ground={.width=1,.soil_depth=1,.surface=&surface};
    dc_level_view_t *v=dc_level_view_create_ground(directory,314,&ground,err,sizeof(err));ASSERT_TRUE(v);
    ASSERT_TRUE(dc_level_view_pan_pixels(v,0,128));
    ASSERT_TRUE(dc_level_view_wait_visible(v,120000,err,sizeof(err)));
    ASSERT_TRUE(dc_players_enable(v,err,sizeof(err)));
    fr_player_t p;fr_beaver_body_t binding;
    ASSERT_TRUE(dc_players_status(v,0,&p,&binding));ASSERT_EQ(p.action,FR_PLAYER_IDLE);
    for(unsigned n=0;n<60;n++) {
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
        ASSERT_TRUE(dc_players_status(v,0,&p,&binding));ASSERT_EQ(p.action,FR_PLAYER_IDLE);
    }
    ASSERT_TRUE(capture(v,"build/agents/beaver-player/sustained/runtime-idle.bmp",err,sizeof(err)));
    ASSERT_TRUE(runtime_visual_check(v,err,sizeof(err)));
    dc_gpu_world_body_t before,after;dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_level_view_body_state(v,binding.binding.body_id,&before,&motion,err,sizeof(err)));
    SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.scancode=SDL_SCANCODE_D;
    ASSERT_TRUE(dc_players_event(v,&e));
    for(unsigned n=0;n<7;n++) ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,0,&p,&binding));ASSERT_EQ(p.action,FR_PLAYER_WALK);ASSERT_TRUE(p.elapsed_ms>=100);
    ASSERT_EQ(p.facing,1);
    ASSERT_TRUE(dc_level_view_body_state(v,binding.binding.body_id,&after,&motion,err,sizeof(err)));
    ASSERT_TRUE(after.chunk.x>before.chunk.x||after.body.x_fp>before.body.x_fp);
    ASSERT_TRUE(capture(v,"build/agents/beaver-player/sustained/runtime-walk.bmp",err,sizeof(err)));
    ASSERT_TRUE(runtime_visual_check(v,err,sizeof(err)));
    e.type=SDL_KEYUP;e.key.keysym.scancode=SDL_SCANCODE_D;ASSERT_TRUE(dc_players_event(v,&e));
    e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_A;ASSERT_TRUE(dc_players_event(v,&e));
    for(unsigned n=0;n<7;n++) ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,0,&p,&binding));ASSERT_EQ(p.action,FR_PLAYER_WALK);ASSERT_EQ(p.facing,-1);
    ASSERT_TRUE(capture(v,"build/agents/beaver-player/sustained/runtime-walk-left.bmp",err,sizeof(err)));
    ASSERT_TRUE(runtime_visual_check(v,err,sizeof(err)));
    e.type=SDL_KEYUP;e.key.keysym.scancode=SDL_SCANCODE_A;ASSERT_TRUE(dc_players_event(v,&e));
    e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;dc_players_event(v,&e);
    ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,0,&p,&binding));ASSERT_EQ(p.action,FR_PLAYER_IDLE);ASSERT_EQ(p.intent.move_x,0);
    e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;dc_players_event(v,&e);
    e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_Q;ASSERT_TRUE(dc_players_event(v,&e));
    for(unsigned n=0;n<24;n++) ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,0,&p,&binding));ASSERT_EQ(p.action,FR_PLAYER_CAST);
    uint32_t frame;ASSERT_TRUE(fr_player_frame(&p,&frame));ASSERT_EQ(frame,5);
    ASSERT_TRUE(capture(v,"build/agents/beaver-player/sustained/runtime-cast.bmp",err,sizeof(err)));
    ASSERT_TRUE(runtime_visual_check(v,err,sizeof(err)));
    for(unsigned n=0;n<120;n++) {
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
        ASSERT_TRUE(dc_players_status(v,0,&p,&binding));
        if(n>=14) ASSERT_EQ(p.action,FR_PLAYER_IDLE);
    }
    e.type=SDL_KEYUP;e.key.keysym.scancode=SDL_SCANCODE_Q;ASSERT_TRUE(dc_players_event(v,&e));
    e.type=SDL_KEYDOWN;ASSERT_TRUE(dc_players_event(v,&e));dc_players_frame_time(v,0);
    ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    const uint32_t intervals[]={79,1,137,3,179,1,79,1,159,1};
    uint32_t action_ms=0;
    for(unsigned n=0;n<10;n++) {
        action_ms+=intervals[n];dc_players_frame_time(v,intervals[n]/1000.);
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
        ASSERT_TRUE(dc_players_status(v,0,&p,&binding));
        if(action_ms<640) {ASSERT_EQ(p.action,FR_PLAYER_CAST);ASSERT_EQ(p.elapsed_ms,action_ms);}
        else ASSERT_EQ(p.action,FR_PLAYER_IDLE);
    }
    ASSERT_TRUE(dc_level_view_destroy(v,err,sizeof(err)));PASS();
}
int main(void) {RUN(test_real_app_keyboard_drives_body_and_animation);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
