#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL.h>
#include "../vulkan/compound_fixture.h"
#include "../../src/app/view_config.h"

static int g_pass,g_fail;
#define RUN(fn) do { printf("RUN  %s\n",#fn); fn(); printf("OK   %s\n",#fn); } while(0)
#define ASSERT_TRUE(e) do { if(!(e)) { printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#e); ++g_fail; return; } } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() ++g_pass
enum { VIS_WIDTH=128,VIS_HEIGHT=96,VIS_TICKS=360 };

static bool snapshot(dc_gpu_t *g,const char *name,uint32_t *pixels,char *err) {
    if(!dc_gpu_readback(g,pixels,VIS_WIDTH*VIS_HEIGHT,err,256)) return false;
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormatFrom(pixels,VIS_WIDTH,VIS_HEIGHT,32,
        VIS_WIDTH*4,SDL_PIXELFORMAT_ABGR8888);
    bool okay=s && SDL_SaveBMP(s,name)==0;
    if(s) SDL_FreeSurface(s);
    return okay;
}
static void test_two_concave_bodies_fall_rotate_collide_and_settle_visibly(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    ASSERT_TRUE(dc_gpu_create_window(&g,SIM_WIDTH,SIM_HEIGHT,1024,768,
        "build/shaders/pattern.comp.spv",err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(uint32_t slot=0;slot<SIM_CHUNKS_X*SIM_CHUNKS_Y;++slot) {
        memset(chunk,0,sizeof(*chunk));
        if(slot/SIM_CHUNKS_X==1)
            for(uint32_t x=0;x<64;++x) chunk->cells[16*64+x].material=DC_MATERIAL_STONE;
        ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,slot%SIM_CHUNKS_X,slot/SIM_CHUNKS_X,slot,err,sizeof(err)));
    }
    free(chunk);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    dc_gpu_compound_shape_t l=fixture_l(),u=fixture_u();
    dc_gpu_world_body_t first=fixture_body(1,50,20),second=fixture_body(2,59,45);
    first.body.vx_fp=32768;
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,first,&l,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,second,&u,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,1,0,.12f,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,2,0,-.06f,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_viewport(g,0,0,VIS_WIDTH,VIS_HEIGHT));
    ASSERT_TRUE(dc_gpu_set_display_zoom(g,8));
    ASSERT_TRUE(dc_gpu_set_window_title(g,"Rigid dynamics: concave stone L + wood U — gravity, rotation, contacts"));
    ASSERT_TRUE(dc_gpu_present_chunks(g,err,sizeof(err)));
    uint32_t *pixels=malloc(VIS_WIDTH*VIS_HEIGHT*sizeof(*pixels));ASSERT_TRUE(pixels);
    mkdir("build/screenshots",0777);
    ASSERT_TRUE(snapshot(g,"build/screenshots/rigid_concave_before.bmp",pixels,err));
    FILE *frames=fopen("build/screenshots/rigid_concave.rgba","wb");ASSERT_TRUE(frames);
    SDL_Delay(1000);
    bool body_contact=false,terrain_contact=false,rotated=false;
    for(uint32_t tick=0;tick<VIS_TICKS;++tick) {
        uint64_t started=SDL_GetTicks64();SDL_Event event;
        while(SDL_PollEvent(&event)) ASSERT_TRUE(event.type!=SDL_QUIT);
        ASSERT_TRUE(dc_gpu_present_chunks_steps(g,1,err,sizeof(err)));
        dc_gpu_contact_stats_t stats;dc_gpu_contact_t contacts[512];
        ASSERT_TRUE(dc_gpu_read_contacts(g,&stats,contacts,512,err,sizeof(err)));
        ASSERT_EQ(stats.overflow,0u);
        for(uint32_t i=0;i<stats.count;++i) {
            if(contacts[i].kind==DC_GPU_CONTACT_BODY) {
                if(!body_contact) ASSERT_TRUE(snapshot(g,"build/screenshots/rigid_concave_impact.bmp",pixels,err));
                body_contact=true;
            }
            terrain_contact=terrain_contact || contacts[i].kind==DC_GPU_CONTACT_TERRAIN;
        }
        for(uint32_t id=1;id<=2;++id) {
            dc_gpu_body_motion_t m;ASSERT_TRUE(dc_gpu_read_body_motion(g,id,&m,err,sizeof(err)));
            ASSERT_TRUE(isfinite(m.angle) && isfinite(m.angular_velocity));
            rotated=rotated || fabsf(m.angle)>.15f;
        }
        if(tick%2==0) {
            ASSERT_TRUE(dc_gpu_readback(g,pixels,VIS_WIDTH*VIS_HEIGHT,err,sizeof(err)));
            ASSERT_EQ(fwrite(pixels,sizeof(*pixels),VIS_WIDTH*VIS_HEIGHT,frames),VIS_WIDTH*VIS_HEIGHT);
        }
        uint64_t elapsed=SDL_GetTicks64()-started;
        if(elapsed<17) SDL_Delay((uint32_t)(17-elapsed));
    }
    ASSERT_EQ(fclose(frames),0);
    ASSERT_TRUE(body_contact && terrain_contact && rotated);
    ASSERT_TRUE(snapshot(g,"build/screenshots/rigid_concave_settled.bmp",pixels,err));
    for(uint32_t id=1;id<=2;++id) {
        dc_gpu_world_body_t b;dc_gpu_body_motion_t m;
        ASSERT_TRUE(dc_gpu_read_world_body(g,id,&b,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(g,id,&m,err,sizeof(err)));
        double y=(double)b.chunk.y*64+b.body.y_fp/65536.0;
        printf("concave body %u: y=%.4f angle=%.4f velocity=(%.4f,%.4f) omega=%.4f\n",
            id,y,m.angle,b.body.vx_fp/65536.0,b.body.vy_fp/65536.0,m.angular_velocity);
        ASSERT_TRUE(y>20 && y<80);
        ASSERT_TRUE(abs(b.body.vx_fp)<13107 && abs(b.body.vy_fp)<13107 && fabsf(m.angular_velocity)<.05f);
    }
    dc_gpu_rigid_solver_stats_t solver;ASSERT_TRUE(dc_gpu_read_rigid_solver_stats(g,&solver));
    ASSERT_EQ(solver.active_bodies,2u);ASSERT_EQ(solver.overflow,0u);
    SDL_Delay(2000);free(pixels);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_two_concave_bodies_fall_rotate_collide_and_settle_visibly);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail ? 1 : 0;
}
