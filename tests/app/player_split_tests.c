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
    if (v && !dc_level_view_destroy(v,err,sizeof(err))) {
        printf("FAIL closing split-screen fixture: %s\n", err);
        g_fail++;
    }
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

static bool world_step(void) {
    return dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err));
}

static bool actor_position(uint32_t seat, dc_gpu_world_body_t *position) {
    fr_player_t player;
    fr_beaver_body_t body;
    return dc_players_status(v,seat,&player,&body)&&
        dc_gpu_read_world_body(dc_level_view_gpu(v),body.binding.body_id,
                               position,err,sizeof(err));
}

static void test_split_camera_keeps_both_moving_players_framed_across_chunks(void) {
    ASSERT_TRUE(setup_at(16,true));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,2,err,sizeof(err)));
    for (uint32_t i=0; i<6; ++i) ASSERT_TRUE(world_step());
    dc_gpu_world_body_t start[2], finish[2];
    ASSERT_TRUE(actor_position(0,&start[0])&&actor_position(1,&start[1]));
    const SDL_Event left = {.key = {.type = SDL_KEYDOWN, .keysym.scancode = SDL_SCANCODE_A}};
    const SDL_Event right = {.caxis = {.type = SDL_CONTROLLERAXISMOTION,
        .which = 42, .axis = SDL_CONTROLLER_AXIS_LEFTX, .value = 32767}};
    ASSERT_TRUE(dc_players_event(v,&left)&&dc_players_event(v,&right));
    for (uint32_t i=0; i<80; ++i) ASSERT_TRUE(world_step());
    ASSERT_TRUE(actor_position(0,&finish[0])&&actor_position(1,&finish[1]));
    ASSERT_TRUE(finish[0].chunk.x < start[0].chunk.x);
    ASSERT_TRUE(finish[1].chunk.x > start[1].chunk.x);

    dc_level_view_status_t camera;
    ASSERT_TRUE(dc_level_view_status(v,&camera));
    int32_t group_x = 0;
    for (uint32_t seat=0; seat<2; ++seat) {
        fr_player_point_t center;
        ASSERT_TRUE(center_target(seat,&center));
        int32_t body_x=finish[seat].body.x_fp/(int32_t)DC_FLUID_FULL;
        int32_t half_width=(int32_t)finish[seat].body.width/2;
        int64_t crop_offset=(center.chunk.x-finish[seat].chunk.x)*DC_CHUNK_SIDE+
            (int32_t)center.x-body_x-half_width;
        ASSERT_TRUE(crop_offset>=-1&&crop_offset<=1);
        group_x+=(int32_t)((finish[seat].chunk.x-camera.origin.x)*DC_CHUNK_SIDE)+
                 body_x+half_width-(int32_t)camera.offset_x;
    }
    ASSERT_TRUE(abs(group_x/2-VIEW_WIDTH/2)<12);

    const SDL_Event jump = {.cbutton = {.type = SDL_CONTROLLERBUTTONDOWN,
        .which = 42, .button = SDL_CONTROLLER_BUTTON_A}};
    ASSERT_TRUE(dc_players_event(v,&jump));
    for (uint32_t i=0; i<5; ++i) ASSERT_TRUE(world_step());
    dc_gpu_world_body_t airborne;
    ASSERT_TRUE(actor_position(1,&airborne));
    int64_t climb = (finish[1].chunk.y-airborne.chunk.y)*DC_CHUNK_SIDE*DC_FLUID_FULL+
        finish[1].body.y_fp-airborne.body.y_fp;
    ASSERT_TRUE(climb > 2 * DC_FLUID_FULL);
    fr_player_point_t controller_center;
    ASSERT_TRUE(center_target(1,&controller_center));
    int64_t y_from_body = (controller_center.chunk.y-airborne.chunk.y)*DC_CHUNK_SIDE+
        (int32_t)controller_center.y-airborne.body.y_fp/(int32_t)DC_FLUID_FULL;
    ASSERT_TRUE(llabs(y_from_body-(int32_t)airborne.body.height/2)<=1);
    PASS();
}

static bool pixel_is_white(int x, int y) {
    char command[160];
    snprintf(command,sizeof(command),"import -window root -crop 1x1+%d+%d -depth 8 rgb:-",x,y);
    FILE *image=popen(command,"r");
    if(!image) return false;
    uint8_t rgb[3];
    size_t bytes=fread(rgb,1,sizeof(rgb),image);
    return pclose(image)==0&&bytes==sizeof(rgb)&&
        rgb[0]>=250&&rgb[1]>=250&&rgb[2]>=250;
}

static void test_aim_reticle_is_drawn_only_in_its_own_window_band(void) {
    ASSERT_TRUE(setup_at(0,false));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,2,err,sizeof(err)));
    SDL_Window *window=SDL_GetKeyboardFocus();
    ASSERT_TRUE(window);
    dc_presentation_layout_t keyboard, controller;
    dc_gpu_t *gpu=dc_level_view_gpu(v);
    ASSERT_TRUE(dc_gpu_presentation_view(gpu,0,&keyboard));
    ASSERT_TRUE(dc_gpu_presentation_view(gpu,1,&controller));
    uint32_t aim_x=controller.x+controller.width/2u;
    uint32_t aim_y=controller.y+controller.height/2u;
    int marker_x=(int)(controller.left+controller.width/2u*controller.scale+controller.scale/2u);
    int marker_y=(int)(controller.top+controller.height/2u*controller.scale+controller.scale/2u);
    SDL_WarpMouseInWindow(window,marker_x,marker_y);
    SDL_PumpEvents();
    ASSERT_TRUE(world_step());
    fr_player_t player;
    fr_beaver_body_t body;
    ASSERT_TRUE(dc_players_status(v,0,&player,&body)&&!player.intent.has_target);
    if (getenv("DISPLAY")) {
        int window_x, window_y;
        SDL_GetWindowPosition(window,&window_x,&window_y);
        SDL_Delay(50);
        ASSERT_TRUE(pixel_is_white(window_x+marker_x+4,window_y+marker_y));
        ASSERT_TRUE(aim_x>=keyboard.x&&aim_x-keyboard.x<keyboard.width);
        ASSERT_TRUE(aim_y>=keyboard.y&&aim_y-keyboard.y<keyboard.height);
        int other_x=(int)(keyboard.left+(aim_x-keyboard.x)*keyboard.scale+keyboard.scale/2u);
        int other_y=(int)(keyboard.top+(aim_y-keyboard.y)*keyboard.scale+keyboard.scale/2u);
        ASSERT_TRUE(!pixel_is_white(window_x+other_x+4,window_y+other_y));
    }
    const SDL_Event disconnect = {.cdevice = {.type = SDL_CONTROLLERDEVICEREMOVED, .which = 42}};
    dc_players_event(v,&disconnect);
    ASSERT_TRUE(world_step());
    ASSERT_TRUE(dc_players_status(v,1,&player,&body)&&!player.intent.has_target);
    PASS();
}

static void test_four_way_split_clamps_stick_to_its_own_crop(void) {
    ASSERT_TRUE(setup_at(0,false));
    for (int32_t device=42; device<45; ++device) {
        const SDL_Event start = {.cbutton = {.type = SDL_CONTROLLERBUTTONDOWN,
            .which = device, .button = SDL_CONTROLLER_BUTTON_START}};
        ASSERT_TRUE(dc_players_event(v,&start));
    }
    dc_presentation_layout_t crop;
    ASSERT_TRUE(dc_gpu_presentation_view(dc_level_view_gpu(v),3,&crop));
    ASSERT_EQ(crop.width,VIEW_WIDTH/4u);
    ASSERT_EQ(crop.height,VIEW_HEIGHT);
    const SDL_Event aim = {.caxis = {.type = SDL_CONTROLLERAXISMOTION,
        .which = 44, .axis = SDL_CONTROLLER_AXIS_RIGHTX, .value = 32767}};
    ASSERT_TRUE(dc_players_event(v,&aim));
    ASSERT_TRUE(dc_players_step(v,1.f/60.f,err,sizeof(err)));
    fr_player_t player;
    fr_beaver_body_t body;
    ASSERT_TRUE(dc_players_status(v,3,&player,&body)&&player.intent.has_target);
    dc_level_view_status_t camera;
    ASSERT_TRUE(dc_level_view_status(v,&camera));
    int64_t viewport_x = (player.intent.target.chunk.x-camera.origin.x)*DC_CHUNK_SIDE+
        (int32_t)player.intent.target.x-(int32_t)camera.offset_x;
    ASSERT_EQ(viewport_x,crop.x+crop.width-1u);
    PASS();
}
int main(void) {
    printf("=== Player Split Screen Tests ===\n\n");
    RUN(test_second_seat_joins_with_its_own_beaver);
    RUN(test_split_screen_routes_bands_to_their_players);
    RUN(test_split_screen_needs_two_seats);
    RUN(test_start_activates_split_and_editor_recovers_full_view);
    RUN(test_controller_aim_uses_its_crop_in_a_far_negative_world);
    RUN(test_split_camera_keeps_both_moving_players_framed_across_chunks);
    RUN(test_aim_reticle_is_drawn_only_in_its_own_window_band);
    RUN(test_four_way_split_clamps_stick_to_its_own_crop);
    printf("\nResults: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
