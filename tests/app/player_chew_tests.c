#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../../src/app/player_session.h"
#include "../../src/app/view_config.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,err);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define PASS() g_pass++
static char err[256];
static dc_level_view_t *v=NULL;

static bool capture(const char *path) {
    mkdir("build/agents",0777);mkdir("build/agents/beaver-player",0777);
    mkdir("build/agents/beaver-player/chew",0777);
    uint32_t *pixels=malloc((size_t)VIEW_WIDTH*VIEW_HEIGHT*4);
    bool okay=pixels&&dc_level_view_pixels(v,pixels,VIEW_WIDTH*VIEW_HEIGHT,err,sizeof(err));
    SDL_Surface *surface=okay?SDL_CreateRGBSurfaceWithFormatFrom(pixels,VIEW_WIDTH,VIEW_HEIGHT,32,VIEW_WIDTH*4,SDL_PIXELFORMAT_ABGR8888):NULL;
    okay=surface&&SDL_SaveBMP(surface,path)==0;
    if(surface) SDL_FreeSurface(surface);
    free(pixels);return okay;
}
static void test_chewing_credits_inventory_in_the_app(void) {
    char directory[]="build/player_chew_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    uint32_t surface=96;
    dc_ground_profile_t ground={.width=1,.soil_depth=1,.surface=&surface};
    v=dc_level_view_create_ground(directory,314,&ground,err,sizeof(err));ASSERT_TRUE(v);
    ASSERT_TRUE(dc_level_view_pan_pixels(v,0,128));
    ASSERT_TRUE(dc_level_view_wait_visible(v,120000,err,sizeof(err)));
    ASSERT_TRUE(dc_players_enable(v,err,sizeof(err)));
    fr_player_t p;fr_beaver_body_t binding;
    ASSERT_TRUE(dc_players_status(v,0,&p,&binding));
    /* Locate the beaver in viewport pixels and raise a wood pillar nearby. */
    dc_gpu_world_body_t self;
    ASSERT_TRUE(dc_gpu_read_world_body(dc_level_view_gpu(v),binding.binding.body_id,&self,err,sizeof(err)));
    int64_t origin_x,origin_y;int32_t camera_x,camera_y;
    dc_level_view_camera(v,&origin_x,&origin_y,&camera_x,&camera_y);
    float px=(float)self.chunk.x*64.f+self.body.x_fp/65536.f-(float)origin_x*64.f-(float)camera_x;
    float py=(float)self.chunk.y*64.f+self.body.y_fp/65536.f-(float)origin_y*64.f-(float)camera_y;
    uint32_t pillar_x=(uint32_t)px+20u,pillar_y=(uint32_t)py;
    dc_gpu_t *gpu=dc_level_view_gpu(v);
    if(pillar_x>=VIEW_WIDTH-2u) pillar_x=VIEW_WIDTH-3u;
    if(pillar_y>=VIEW_HEIGHT-2u) pillar_y=VIEW_HEIGHT-3u;
    ASSERT_TRUE(dc_gpu_paint_material(gpu,pillar_x,pillar_y,3u,DC_MATERIAL_WOOD,err,sizeof(err)));
    {
        dc_chunk_t *scan=calloc(1,sizeof(*scan));
        for(uint32_t slot=0u;slot<64u;slot++) {
            const uint32_t *registry=dc_gpu_structural_data(gpu)+32u+slot*8u;
            if(!registry[5]) continue;
            ASSERT_TRUE(dc_gpu_download_chunk(gpu,slot,scan,err,sizeof(err)));
            unsigned wood=0u;
            for(uint32_t c=0;c<4096u;c++)
                if(scan->cells[c].material==DC_MATERIAL_WOOD) wood++;
            if(wood)
                printf("chunk %lld,%lld slot %u wood cells %u\n",
                    (long long)scan->coord.x,(long long)scan->coord.y,slot,wood);
        }
        free(scan);
    }
    /* Pin the aim at the painted pillar's real cell: scan the resident
     * chunks for the wood the brush just placed. */
    fr_player_point_t aim={0};
    {
        dc_chunk_t *scan=calloc(1,sizeof(*scan));
        for(uint32_t slot=0u;slot<64u&&!aim.x&&slot<64u;slot++) {
            const uint32_t *registry=dc_gpu_structural_data(gpu)+32u+slot*8u;
            if(!registry[5]) continue;
            ASSERT_TRUE(dc_gpu_download_chunk(gpu,slot,scan,err,sizeof(err)));
            for(uint32_t c=0;c<4096u&&!aim.x;c++)
                if(scan->cells[c].material==DC_MATERIAL_WOOD) {
                    aim.chunk=scan->coord;
                    aim.x=(float)(c%64u);aim.y=(float)(c/64u);
                }
        }
        free(scan);
    }
    ASSERT_TRUE(aim.x>0.f);
    printf("aim pinned at chunk %lld,%lld cell %.0f,%.0f\n",
        (long long)aim.chunk.x,(long long)aim.chunk.y,aim.x,aim.y);
    ASSERT_TRUE(dc_players_aim_override(v,0,&aim,err,sizeof(err)));
    SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.scancode=SDL_SCANCODE_C;
    ASSERT_TRUE(dc_players_event(v,&e));
    ASSERT_TRUE(capture("build/agents/beaver-player/chew/chew-start.bmp"));
    /* One long deterministic frame completes bites immediately, before
     * background streaming can regenerate the painted pillar. */
    for(unsigned n=0;n<6;n++) {
        dc_players_frame_time(v,0.5);
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    }
    fr_player_inventory_t early;
    ASSERT_TRUE(dc_players_inventory(v,0,&early));
    printf("chew early inventory wood %llu fp\n",
        (unsigned long long)early.mass_fp[DC_ITEM_WOOD]);
    ASSERT_TRUE(early.mass_fp[DC_ITEM_WOOD]>0u);
    for(unsigned n=0;n<240;n++) {
        dc_players_frame_time(v,1./60.);
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    }
    e.type=SDL_KEYUP;ASSERT_TRUE(dc_players_event(v,&e));
    fr_player_inventory_t inv;
    ASSERT_TRUE(dc_players_inventory(v,0,&inv));
    printf("chew app inventory wood %llu fp\n",(unsigned long long)inv.mass_fp[DC_ITEM_WOOD]);
    ASSERT_TRUE(inv.mass_fp[DC_ITEM_WOOD]>0u);
    ASSERT_TRUE(capture("build/agents/beaver-player/chew/chew-end.bmp"));
    dc_level_view_destroy(v,err,sizeof(err));v=NULL;
    PASS();
}
int main(void) {
    printf("=== Player Chew App Tests ===\n\n");
    RUN(test_chewing_credits_inventory_in_the_app);
    printf("\nResults: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
