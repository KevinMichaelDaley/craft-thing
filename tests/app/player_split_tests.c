#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../../src/app/player_session.h"
#include "../../src/app/view_config.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();teardown();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,err);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static char err[256];
static dc_level_view_t *v=NULL;
static SDL_Window *cover_window;
static uint32_t *before_pixels, *after_pixels;
enum { RETICLE_ARM_SAMPLE = 4, RGB_WHITE = 0x00ffffff };

static bool capture_window(dc_gpu_t *gpu, uint32_t **pixels) {
    size_t count = (size_t)gpu->swap_extent.width * gpu->swap_extent.height;
    if (!*pixels) *pixels = malloc(count * sizeof(**pixels));
    return *pixels && dc_gpu_capture_present(gpu, *pixels, count, err, sizeof(err));
}

static bool save_window(dc_gpu_t *gpu, uint32_t *pixels, const char *path) {
    int width = (int)gpu->swap_extent.width, height = (int)gpu->swap_extent.height;
    SDL_Surface *image = SDL_CreateRGBSurfaceWithFormatFrom(pixels, width, height, 32,
        width * (int)sizeof(*pixels), SDL_PIXELFORMAT_ABGR8888);
    if (!image) return false;
    bool okay = SDL_SaveBMP(image, path) == 0;
    SDL_FreeSurface(image);
    return okay;
}

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
    if (cover_window) SDL_DestroyWindow(cover_window);
    cover_window = NULL;
    free(before_pixels);
    free(after_pixels);
    before_pixels = NULL;
    after_pixels = NULL;
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

static bool band_changed(const dc_gpu_t *gpu, const dc_presentation_layout_t *band) {
    uint32_t width = band->width * band->scale;
    for (uint32_t row = 0; row < band->height * band->scale; ++row) {
        size_t at = (size_t)(band->top + row) * gpu->swap_extent.width + band->left;
        if (memcmp(before_pixels + at, after_pixels + at, width * sizeof(*before_pixels)))
            return true;
    }
    return false;
}

static void test_split_screen_routes_bands_to_their_players(void) {
    ASSERT_TRUE(setup_at(0,true));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    for(unsigned n=0;n<4;n++)
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,2,err,sizeof(err)));
    dc_gpu_t *gpu = dc_level_view_gpu(v);
    dc_presentation_layout_t band;
    ASSERT_TRUE(dc_gpu_presentation_view(gpu,0,&band));
    ASSERT_TRUE(band.scale >= WINDOW_SCALE);
    uint32_t left_x, right_x, y, x;
    ASSERT_TRUE(dc_gpu_screen_cell(gpu,0,0,&left_x,&y));
    ASSERT_TRUE(dc_gpu_screen_cell(gpu,band.scale,0,&x,&y));
    ASSERT_EQ(x,left_x+1u);
    ASSERT_TRUE(dc_gpu_screen_cell(gpu,VIEW_WIDTH*WINDOW_SCALE/2u,0,&right_x,&y));
    ASSERT_TRUE(right_x>left_x);
    ASSERT_TRUE(dc_level_view_tick(v,err,sizeof(err)) && capture_window(gpu,&before_pixels));
    dc_level_view_status_t before, after;
    ASSERT_TRUE(dc_gpu_presentation_view(gpu,0,&band) && dc_level_view_status(v,&before));
    SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.scancode=SDL_SCANCODE_D;
    ASSERT_TRUE(dc_players_event(v,&e));
    for (unsigned n=0;n<40;n++)
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    e.type=SDL_KEYUP;ASSERT_TRUE(dc_players_event(v,&e));
    ASSERT_TRUE(capture_window(gpu,&after_pixels));
    dc_presentation_layout_t walked;
    ASSERT_TRUE(dc_gpu_presentation_view(gpu,0,&walked) && dc_level_view_status(v,&after));
    int64_t before_x = before.origin.x * DC_CHUNK_SIDE + before.offset_x + band.x;
    int64_t after_x = after.origin.x * DC_CHUNK_SIDE + after.offset_x + walked.x;
    ASSERT_TRUE(before_x != after_x && band_changed(gpu,&band));
    mkdir("build/agents",0777);mkdir("build/agents/beaver-player",0777);
    mkdir("build/agents/beaver-player/split",0777);
    ASSERT_TRUE(save_window(gpu,before_pixels,"build/agents/beaver-player/split/split-start.bmp"));
    ASSERT_TRUE(save_window(gpu,after_pixels,"build/agents/beaver-player/split/split-walk.bmp"));
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
    ASSERT_TRUE(x < VIEW_WIDTH/4u);
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
    dc_presentation_layout_t crop;
    ASSERT_TRUE(dc_gpu_presentation_view(dc_level_view_gpu(v),1,&crop));
    uint32_t reach = crop.width - 1u - crop.width/2u;
    ASSERT_EQ((player.intent.target.chunk.x-center.chunk.x)*DC_CHUNK_SIDE+
        (int32_t)player.intent.target.x-(int32_t)center.x,reach < 80u ? reach : 80u);
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

static bool read_window_pixel(const dc_gpu_t *gpu, const uint32_t *pixels,
                              uint32_t x, uint32_t y, uint32_t *out) {
    if (x >= gpu->swap_extent.width || y >= gpu->swap_extent.height) return false;
    *out = pixels[(size_t)y * gpu->swap_extent.width + x];
    return true;
}

static bool cover_player_window(SDL_Window *window) {
    int x, y, width, height;
    SDL_GetWindowPosition(window, &x, &y);
    SDL_GetWindowSize(window, &width, &height);
    cover_window = SDL_CreateWindow("Split-screen overlap fixture", x, y, width, height,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN | SDL_WINDOW_ALWAYS_ON_TOP);
    if (!cover_window) return false;
    SDL_Surface *surface = SDL_GetWindowSurface(cover_window);
    if (!surface || SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0, 0, 0)) < 0 ||
        SDL_UpdateWindowSurface(cover_window) < 0) return false;
    SDL_RaiseWindow(cover_window);
    return true;
}

static void test_gameplay_ignores_other_window_focus_and_keys(void) {
    ASSERT_TRUE(setup_at(0,false));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    SDL_Window *game = dc_level_view_gpu(v)->window;
    ASSERT_TRUE(game && cover_player_window(game));
    uint32_t owned = SDL_GetWindowID(game), other = SDL_GetWindowID(cover_window);
    ASSERT_TRUE(owned && other && owned != other);

    const SDL_Event lost_game = {.window = {.type = SDL_WINDOWEVENT, .windowID = owned,
        .event = SDL_WINDOWEVENT_FOCUS_LOST}};
    const SDL_Event gain_other = {.window = {.type = SDL_WINDOWEVENT, .windowID = other,
        .event = SDL_WINDOWEVENT_FOCUS_GAINED}};
    dc_players_event(v,&lost_game);
    dc_players_event(v,&gain_other);
    fr_player_t keyboard, controller;
    fr_beaver_body_t body;
    ASSERT_TRUE(dc_players_step(v,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,1,&controller,&body) && !controller.intent.has_target);

    const SDL_Event gain_game = {.window = {.type = SDL_WINDOWEVENT, .windowID = owned,
        .event = SDL_WINDOWEVENT_FOCUS_GAINED}};
    const SDL_Event lost_other = {.window = {.type = SDL_WINDOWEVENT, .windowID = other,
        .event = SDL_WINDOWEVENT_FOCUS_LOST}};
    dc_players_event(v,&gain_game);
    dc_players_event(v,&lost_other);
    SDL_Event key = {.key = {.type = SDL_KEYDOWN, .windowID = other,
        .keysym.scancode = SDL_SCANCODE_D}};
    dc_players_event(v,&key);
    ASSERT_TRUE(dc_players_step(v,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,1,&controller,&body) && controller.intent.has_target);
    ASSERT_TRUE(dc_players_status(v,0,&keyboard,&body) && keyboard.intent.move_x == 0.f);
    key.key.windowID = owned;
    ASSERT_TRUE(dc_players_event(v,&key));
    ASSERT_TRUE(dc_players_step(v,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,0,&keyboard,&body) && keyboard.intent.move_x == 1.f);

    key.key.keysym.scancode = SDL_SCANCODE_TAB;
    key.key.windowID = other;
    dc_players_event(v,&key);
    ASSERT_TRUE(dc_players_gameplay(v));
    key.key.windowID = owned;
    ASSERT_TRUE(dc_players_event(v,&key) && !dc_players_gameplay(v));

    SDL_Event text = {.text = {.type = SDL_TEXTINPUT, .windowID = other, .text = "2718"}};
    ASSERT_TRUE(!dc_level_view_accepts_event(v,&text));
    text.text.windowID = owned;
    ASSERT_TRUE(dc_level_view_accepts_event(v,&text));
    text.text.windowID = 0u;
    ASSERT_TRUE(dc_level_view_accepts_event(v,&text));
    PASS();
}

static bool point_inside_window(SDL_Window *window,int x,int y) {
    SDL_WarpMouseInWindow(window,x,y);
    for (uint32_t i=0; i<20u; ++i) {
        SDL_PumpEvents();
        int mx,my;
        SDL_GetMouseState(&mx,&my);
        if (SDL_GetMouseFocus()==window && mx==x && my==y) return true;
        SDL_Delay(10u);
    }
    return false;
}

static void test_pointer_in_other_window_cannot_target_a_player(void) {
    ASSERT_TRUE(setup_at(0,false));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,2,err,sizeof(err)));
    dc_gpu_t *gpu=dc_level_view_gpu(v);
    SDL_Window *game=gpu->window;
    dc_presentation_layout_t keyboard;
    ASSERT_TRUE(game && dc_gpu_presentation_view(gpu,0,&keyboard));
    int x = (int)(keyboard.left + (keyboard.width / 2u) * keyboard.scale);
    int y = (int)(keyboard.top + (keyboard.height / 2u) * keyboard.scale);
    ASSERT_TRUE(cover_player_window(game) && point_inside_window(cover_window,x,y));
    ASSERT_TRUE(dc_players_step(v,1.f/60.f,err,sizeof(err)));
    fr_player_t player;
    fr_beaver_body_t body;
    ASSERT_TRUE(dc_players_status(v,0,&player,&body) && !player.intent.has_target);
    ASSERT_TRUE(dc_players_status(v,1,&player,&body) && player.intent.has_target);
    SDL_DestroyWindow(cover_window);
    cover_window=NULL;
    ASSERT_TRUE(point_inside_window(game,x,y));
    ASSERT_TRUE(dc_players_step(v,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,0,&player,&body) && player.intent.has_target);
    PASS();
}

static void test_aim_reticle_is_drawn_only_in_its_own_window_band(void) {
    ASSERT_TRUE(setup_at(0,false));
    ASSERT_TRUE(dc_players_join(v,42,err,sizeof(err)));
    ASSERT_TRUE(dc_players_split_screen(v,2,err,sizeof(err)));
    dc_presentation_layout_t keyboard, controller;
    dc_gpu_t *gpu=dc_level_view_gpu(v);
    SDL_Window *window = gpu->window;
    ASSERT_TRUE(window && cover_player_window(window));
    ASSERT_TRUE(dc_gpu_presentation_view(gpu,0,&keyboard));
    ASSERT_TRUE(dc_gpu_presentation_view(gpu,1,&controller));
    uint32_t marker_x=controller.left+controller.width/2u*controller.scale+controller.scale/2u;
    uint32_t marker_y=controller.top+controller.height/2u*controller.scale+controller.scale/2u;
    SDL_WarpMouseInWindow(cover_window,(int)marker_x,(int)marker_y);
    SDL_PumpEvents();
    ASSERT_TRUE(world_step());
    fr_player_t player;
    fr_beaver_body_t body;
    ASSERT_TRUE(dc_players_status(v,0,&player,&body)&&!player.intent.has_target);
    ASSERT_TRUE(dc_gpu_presentation_view(gpu,0,&keyboard));
    ASSERT_TRUE(dc_gpu_presentation_view(gpu,1,&controller));
    uint32_t aim_x=controller.x+controller.width/2u;
    uint32_t aim_y=controller.y+controller.height/2u;
    ASSERT_TRUE(aim_x>=keyboard.x&&aim_x-keyboard.x<keyboard.width);
    ASSERT_TRUE(aim_y>=keyboard.y&&aim_y-keyboard.y<keyboard.height);
    uint32_t other_x=keyboard.left+(aim_x-keyboard.x)*keyboard.scale+keyboard.scale/2u;
    uint32_t other_y=keyboard.top+(aim_y-keyboard.y)*keyboard.scale+keyboard.scale/2u;
    uint32_t aim_pixel, other_pixel;
    ASSERT_TRUE(capture_window(gpu,&before_pixels));
    ASSERT_TRUE(read_window_pixel(gpu,before_pixels,marker_x+RETICLE_ARM_SAMPLE,marker_y,&aim_pixel));
    ASSERT_TRUE(read_window_pixel(gpu,before_pixels,other_x+RETICLE_ARM_SAMPLE,other_y,&other_pixel));
    ASSERT_EQ(aim_pixel & RGB_WHITE, RGB_WHITE);
    ASSERT_TRUE((other_pixel & RGB_WHITE) != RGB_WHITE);
    const SDL_Event disconnect = {.cdevice = {.type = SDL_CONTROLLERDEVICEREMOVED, .which = 42}};
    dc_players_event(v,&disconnect);
    ASSERT_TRUE(world_step());
    ASSERT_TRUE(dc_players_status(v,1,&player,&body)&&!player.intent.has_target);
    ASSERT_TRUE(capture_window(gpu,&after_pixels));
    ASSERT_TRUE(read_window_pixel(gpu,after_pixels,marker_x+RETICLE_ARM_SAMPLE,marker_y,&aim_pixel));
    ASSERT_TRUE((aim_pixel & RGB_WHITE) != RGB_WHITE);
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
    ASSERT_TRUE(crop.width < VIEW_WIDTH/4u);
    ASSERT_TRUE(crop.height < VIEW_HEIGHT);
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
int main(int argc, char **argv) {
    printf("=== Player Split Screen Tests ===\n\n");
    if (argc == 2 && strcmp(argv[1], "--reticle") == 0) {
        RUN(test_aim_reticle_is_drawn_only_in_its_own_window_band);
        printf("\nResults: %d passed, %d failed\n",g_pass,g_fail);
        return g_fail ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--window-input") == 0) {
        RUN(test_gameplay_ignores_other_window_focus_and_keys);
        RUN(test_pointer_in_other_window_cannot_target_a_player);
        printf("\nResults: %d passed, %d failed\n",g_pass,g_fail);
        return g_fail ? 1 : 0;
    }
    RUN(test_second_seat_joins_with_its_own_beaver);
    RUN(test_split_screen_routes_bands_to_their_players);
    RUN(test_split_screen_needs_two_seats);
    RUN(test_start_activates_split_and_editor_recovers_full_view);
    RUN(test_controller_aim_uses_its_crop_in_a_far_negative_world);
    RUN(test_split_camera_keeps_both_moving_players_framed_across_chunks);
    RUN(test_gameplay_ignores_other_window_focus_and_keys);
    RUN(test_pointer_in_other_window_cannot_target_a_player);
    RUN(test_aim_reticle_is_drawn_only_in_its_own_window_band);
    RUN(test_four_way_split_clamps_stick_to_its_own_crop);
    printf("\nResults: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
