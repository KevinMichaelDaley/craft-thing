#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../../src/app/player_session.h"
#include "../../src/app/view_config.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();teardown();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,err);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static char err[256];
static dc_level_view_t *v=NULL;

static bool setup_at(int32_t pages_west, bool follow) {
    char directory[]="build/player_split_XXXXXX";
    if (!mkdtemp(directory)) return false;
    uint32_t surface=96;
    dc_ground_profile_t ground={.width=1,.soil_depth=1,.surface=&surface};
    v=dc_level_view_create_ground(directory,314,&ground,err,sizeof(err));
    if (!v || !dc_level_view_pan_pixels(v, 0, 128)) return false;
    for (int32_t i = 0; i < pages_west; i += 16)
        if (!dc_level_view_pan_pixels(v, -1024, 0)) return false;
    return dc_level_view_wait_visible(v,120000,err,sizeof(err)) &&
           dc_players_enable_at(v,64,40,follow,err,sizeof(err));
}
static void teardown(void) {
    if(v) dc_level_view_destroy(v,err,sizeof(err));
    v=NULL;
}
static void test_second_seat_joins_with_its_own_beaver(void) {
    ASSERT_TRUE(setup_at(0,true));
    fr_player_t p;fr_beaver_body_t b0,b1;
    ASSERT_TRUE(dc_players_status(v,0,&p,&b0));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,1,&p,&b1));
    ASSERT_TRUE(b1.binding.body_id!=0u);
    ASSERT_TRUE(b1.binding.body_id!=b0.binding.body_id);
    /* The joined seat rides its own input device. */
    ASSERT_TRUE(p.identity!=0u);
    for(unsigned n=0;n<4;n++)
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    PASS();
}
static void test_split_screen_routes_bands_to_their_players(void) {
    ASSERT_TRUE(setup_at(0,true));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    for(unsigned n=0;n<4;n++)
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,2,err,sizeof(err)));
    dc_gpu_t *gpu = dc_level_view_gpu(v);
    uint32_t left_x, right_x, y, x;
    ASSERT_TRUE(dc_gpu_screen_cell(gpu,0,0,&left_x,&y));
    ASSERT_TRUE(dc_gpu_screen_cell(gpu,WINDOW_SCALE,0,&x,&y));
    ASSERT_EQ(x,left_x+1u);
    ASSERT_TRUE(dc_gpu_screen_cell(gpu,VIEW_WIDTH*WINDOW_SCALE/2u,0,&right_x,&y));
    ASSERT_TRUE(right_x>left_x);
    /* Rendered acceptance evidence: walk the keyboard player under a real
     * display server and grab the presented split window. */
    if(getenv("DISPLAY")) {
        ASSERT_TRUE(dc_level_view_set_title(v,"player-split-tests"));
        mkdir("build/agents",0777);mkdir("build/agents/beaver-player",0777);
        mkdir("build/agents/beaver-player/split",0777);
        int start_grab=system("import -window root "
            "build/agents/beaver-player/split/split-start.png 2>/dev/null");
        ASSERT_TRUE(start_grab==0);
        SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.scancode=SDL_SCANCODE_D;
        ASSERT_TRUE(dc_players_event(v,&e));
        for(unsigned n=0;n<40;n++)
            ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
        e.type=SDL_KEYUP;ASSERT_TRUE(dc_players_event(v,&e));
        int grabbed=system("import -window root "
            "build/agents/beaver-player/split/split-walk.png 2>/dev/null");
        ASSERT_TRUE(grabbed==0);
        struct stat st;
        ASSERT_TRUE(stat("build/agents/beaver-player/split/split-walk.png",&st)==0&&st.st_size>1000);
        printf("split evidence %ld bytes\n",(long)st.st_size);
        /* The left viewport follows its player: the walk shifts its crop. */
        int moved=system("cmp -s build/agents/beaver-player/split/split-start.png "
            "build/agents/beaver-player/split/split-walk.png");
        ASSERT_TRUE(moved!=0);
        printf("viewport crop followed the walking player\n");
    }
    ASSERT_TRUE(dc_players_split_screen(v,0,err,sizeof(err)));
    PASS();
}
static void test_split_screen_needs_two_seats(void) {
    ASSERT_TRUE(setup_at(0,true));
    ASSERT_TRUE(!dc_players_split_screen(v,2,err,sizeof(err)));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,2,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,0,err,sizeof(err)));
    PASS();
}

static bool center_target(unsigned seat, fr_player_point_t *point) {
    uint32_t x, y, local_x, local_y;
    dc_level_view_status_t status;
    unsigned screen_x = (seat * 2u + 1u) * VIEW_WIDTH * WINDOW_SCALE / 4u;
    if (!dc_gpu_screen_cell(dc_level_view_gpu(v), screen_x,
                            VIEW_HEIGHT * WINDOW_SCALE / 2u, &x, &y) ||
        !dc_level_view_status(v, &status)) return false;
    dc_cell_address((int64_t)x + status.offset_x, (int64_t)y + status.offset_y,
                    &point->chunk, &local_x, &local_y);
    point->chunk.x += status.origin.x;
    point->chunk.y += status.origin.y;
    point->x = (float)local_x;
    point->y = (float)local_y;
    return true;
}

static void test_start_activates_split_and_editor_recovers_full_view(void) {
    ASSERT_TRUE(setup_at(0,false));
    const SDL_Event start = {.cbutton = {.type = SDL_CONTROLLERBUTTONDOWN,
        .which = 42, .button = SDL_CONTROLLER_BUTTON_START}};
    ASSERT_TRUE(dc_players_event(v, &start));
    ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    uint32_t x, y;
    ASSERT_TRUE(dc_gpu_screen_cell(dc_level_view_gpu(v),
                                   VIEW_WIDTH*WINDOW_SCALE/4u, 0, &x, &y));
    ASSERT_EQ(x, VIEW_WIDTH/4u);
    ASSERT_TRUE(dc_gpu_screen_cell(dc_level_view_gpu(v),
                                   VIEW_WIDTH*WINDOW_SCALE*3u/4u, 0, &x, &y));
    ASSERT_TRUE(x < VIEW_WIDTH/2u);
    const SDL_Event tab = {.key = {.type = SDL_KEYDOWN, .keysym.scancode = SDL_SCANCODE_TAB}};
    ASSERT_TRUE(dc_players_event(v, &tab));
    ASSERT_TRUE(dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_screen_cell(dc_level_view_gpu(v),
                                   VIEW_WIDTH*WINDOW_SCALE*3u/4u, 0, &x, &y));
    ASSERT_EQ(x, VIEW_WIDTH*3u/4u);
    ASSERT_TRUE(dc_players_event(v, &tab));
    ASSERT_TRUE(dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_screen_cell(dc_level_view_gpu(v),
                                   VIEW_WIDTH*WINDOW_SCALE*3u/4u, 0, &x, &y));
    ASSERT_TRUE(x < VIEW_WIDTH/2u);
    PASS();
}

static void test_controller_aim_uses_its_crop_in_a_far_negative_world(void) {
    ASSERT_TRUE(setup_at(1 << 20,false));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,2,err,sizeof(err)));
    fr_player_point_t center;
    ASSERT_TRUE(center_target(1,&center));
    ASSERT_TRUE(center.chunk.x < -1000000);
    fr_player_t player;
    fr_beaver_body_t body;
    ASSERT_TRUE(dc_players_status(v,1,&player,&body));
    dc_gpu_world_body_t physical;
    ASSERT_TRUE(dc_gpu_read_world_body(dc_level_view_gpu(v),body.binding.body_id,
                                       &physical,err,sizeof(err)));
    int64_t crop_from_actor = (center.chunk.x-physical.chunk.x)*DC_CHUNK_SIDE +
        (int32_t)center.x - physical.body.x_fp/(int32_t)DC_FLUID_FULL;
    ASSERT_INT_EQ(crop_from_actor, physical.body.width/2u);

    ASSERT_TRUE(dc_players_step(v,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,1,&player,&body));
    ASSERT_TRUE(player.intent.has_target);
    ASSERT_EQ(player.intent.target.chunk.x,center.chunk.x);
    ASSERT_EQ(player.intent.target.chunk.y,center.chunk.y);
    ASSERT_EQ(player.intent.target.x,center.x);
    ASSERT_EQ(player.intent.target.y,center.y);

    const SDL_Event aim = {.caxis = {.type = SDL_CONTROLLERAXISMOTION,
        .which = 42, .axis = SDL_CONTROLLER_AXIS_RIGHTX, .value = 32767}};
    ASSERT_TRUE(dc_players_event(v,&aim));
    ASSERT_TRUE(dc_players_step(v,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,1,&player,&body));
    ASSERT_TRUE(player.intent.has_target);
    ASSERT_EQ((player.intent.target.chunk.x-center.chunk.x)*DC_CHUNK_SIDE+
        (int32_t)player.intent.target.x-(int32_t)center.x,80);
    PASS();
}
int main(void) {
    printf("=== Player Split Screen Tests ===\n\n");
    RUN(test_second_seat_joins_with_its_own_beaver);
    RUN(test_split_screen_routes_bands_to_their_players);
    RUN(test_split_screen_needs_two_seats);
    RUN(test_start_activates_split_and_editor_recovers_full_view);
    RUN(test_controller_aim_uses_its_crop_in_a_far_negative_world);
    printf("\nResults: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
