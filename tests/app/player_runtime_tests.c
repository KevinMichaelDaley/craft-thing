#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "../../src/app/level_internal.h"
#include "../../src/app/player_session.h"
#include "../../src/app/view_config.h"
#include "../../src/vulkan/gas_gpu_internal.h"
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
static void test_enabling_player_admits_only_the_player_body(void) {
    char directory[] = "build/player_runtime_scene_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    uint32_t surface = 96;
    dc_ground_profile_t ground = {.width = 1, .soil_depth = 1, .surface = &surface};
    dc_level_view_t *v = dc_level_view_create_ground(directory, 314, &ground,
                                                      err, sizeof(err));
    ASSERT_TRUE(v);
    dc_gpu_t *gpu = dc_level_view_gpu(v);
    dc_gpu_extraction_stats_t before, after;
    bool admitted = dc_level_view_pan_pixels(v, 0, 128) &&
                    dc_level_view_wait_visible(v, 120000, err, sizeof(err)) &&
                    dc_gpu_read_extraction_stats(gpu, &before) &&
                    dc_players_enable(v, err, sizeof(err)) &&
                    dc_gpu_read_extraction_stats(gpu, &after);
    char shutdown_error[256] = {0};
    bool destroyed = dc_level_view_destroy(v, shutdown_error, sizeof(shutdown_error));
    ASSERT_TRUE(admitted);
    if (!destroyed) snprintf(err, sizeof(err), "%s", shutdown_error);
    ASSERT_TRUE(destroyed);
    ASSERT_EQ(after.owned_bodies, before.owned_bodies + 1u);
    PASS();
}

static void test_editor_camera_holds_its_manual_position(void) {
    char directory[] = "build/player_runtime_editor_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    uint32_t surface = 96;
    dc_ground_profile_t ground = {.width = 1, .soil_depth = 1, .surface = &surface};
    dc_level_view_t *v = dc_level_view_create_ground(directory, 314, &ground,
                                                      err, sizeof(err));
    ASSERT_TRUE(v);
    ASSERT_TRUE(dc_level_view_pan_pixels(v, 0, 128));
    ASSERT_TRUE(dc_level_view_wait_visible(v, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_players_enable(v, err, sizeof(err)));
    SDL_Event tab = {.type = SDL_KEYDOWN};
    tab.key.keysym.scancode = SDL_SCANCODE_TAB;
    ASSERT_TRUE(dc_players_event(v, &tab));
    ASSERT_TRUE(!dc_players_gameplay(v));
    ASSERT_TRUE(dc_level_view_pan_pixels(v, 31, -11));
    ASSERT_TRUE(dc_level_view_wait_visible(v, 120000, err, sizeof(err)));
    dc_level_view_status_t manual, editor, gameplay;
    ASSERT_TRUE(dc_level_view_status(v, &manual));
    ASSERT_TRUE(dc_level_view_step(v, err, sizeof(err)) &&
                dc_level_view_tick(v, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_status(v, &editor));
    ASSERT_TRUE(dc_players_event(v, &tab) && dc_players_gameplay(v));
    ASSERT_TRUE(dc_level_view_step(v, err, sizeof(err)) &&
                dc_level_view_tick(v, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_status(v, &gameplay));
    ASSERT_TRUE(dc_level_view_destroy(v, err, sizeof(err)));
    ASSERT_EQ(editor.origin.x, manual.origin.x);
    ASSERT_EQ(editor.origin.y, manual.origin.y);
    ASSERT_EQ(editor.offset_x, manual.offset_x);
    ASSERT_EQ(editor.offset_y, manual.offset_y);
    ASSERT_TRUE(gameplay.origin.x != editor.origin.x ||
                gameplay.offset_x != editor.offset_x);
    PASS();
}

static void test_real_app_keyboard_drives_body_and_animation(void) {
    enum { WALK_FRAMES = 6, WALK_LOOP_TICKS = 36 };
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
    ASSERT_TRUE(runtime_visual_check(v,FR_PLAYER_IDLE,0u,0u,err,sizeof(err)));
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
    uint32_t frame;
    ASSERT_TRUE(fr_player_frame(&p, &frame));
    ASSERT_TRUE(frame < WALK_FRAMES);
    ASSERT_TRUE(runtime_visual_check(v,FR_PLAYER_WALK,frame,0u,err,sizeof(err)));
    uint32_t displayed_walk_frames = 1u << frame;
    const uint32_t all_walk_frames = (1u << WALK_FRAMES) - 1u;
    for (unsigned n = 0; n < WALK_LOOP_TICKS && displayed_walk_frames != all_walk_frames; n++) {
        ASSERT_TRUE(dc_level_view_step(v, err, sizeof(err)) &&
                    dc_level_view_tick(v, err, sizeof(err)));
        ASSERT_TRUE(dc_players_status(v, 0, &p, &binding));
        ASSERT_EQ(p.action, FR_PLAYER_WALK);
        ASSERT_TRUE(fr_player_frame(&p, &frame));
        ASSERT_TRUE(frame < WALK_FRAMES);
        if (!(displayed_walk_frames & (1u << frame))) {
            ASSERT_TRUE(runtime_visual_check(v, FR_PLAYER_WALK, frame, 0u, err, sizeof(err)));
            displayed_walk_frames |= 1u << frame;
        }
    }
    ASSERT_EQ(displayed_walk_frames, all_walk_frames);
    e.type=SDL_KEYUP;e.key.keysym.scancode=SDL_SCANCODE_D;ASSERT_TRUE(dc_players_event(v,&e));
    e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_A;ASSERT_TRUE(dc_players_event(v,&e));
    for(unsigned n=0;n<7;n++) ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,0,&p,&binding));ASSERT_EQ(p.action,FR_PLAYER_WALK);ASSERT_EQ(p.facing,-1);
    ASSERT_TRUE(capture(v,"build/agents/beaver-player/sustained/runtime-walk-left.bmp",err,sizeof(err)));
    ASSERT_TRUE(fr_player_frame(&p,&frame));
    ASSERT_TRUE(runtime_visual_check(v,FR_PLAYER_WALK,frame,0u,err,sizeof(err)));
    e.type=SDL_KEYUP;e.key.keysym.scancode=SDL_SCANCODE_A;ASSERT_TRUE(dc_players_event(v,&e));
    e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;dc_players_event(v,&e);
    ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
    ASSERT_TRUE(dc_players_status(v,0,&p,&binding));ASSERT_EQ(p.action,FR_PLAYER_IDLE);ASSERT_EQ(p.intent.move_x,0);
    e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;dc_players_event(v,&e);
    dc_gpu_world_body_t caster;
    ASSERT_TRUE(dc_level_view_body_state(v,binding.binding.body_id,&caster,&motion,err,sizeof(err)));
    dc_chunk_coord_t aim_offset;
    uint32_t aim_x, aim_y;
    dc_cell_address(caster.body.x_fp/(int32_t)DC_FLUID_FULL-48,
        caster.body.y_fp/(int32_t)DC_FLUID_FULL+caster.body.height/2u, &aim_offset, &aim_x, &aim_y);
    fr_player_point_t aim = {{caster.chunk.x+aim_offset.x,caster.chunk.y+aim_offset.y},
        (float)aim_x,(float)aim_y};
    ASSERT_TRUE(dc_players_aim_override(v,0,&aim,err,sizeof(err)));
    uint32_t projectile_id=dc_gpu_next_body_id(dc_level_view_gpu(v));
    e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_Q;ASSERT_TRUE(dc_players_event(v,&e));
    unsigned cast_frames = 0;
    for(unsigned n=0;n<24;n++) {
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
        ASSERT_TRUE(dc_players_status(v,0,&p,&binding) && p.action == FR_PLAYER_CAST);
        ASSERT_TRUE(fr_player_frame(&p,&frame) && frame < 8u);
        if (!(cast_frames & (1u << frame))) {
            ASSERT_TRUE(runtime_visual_check(v, FR_PLAYER_CAST, frame, projectile_id, err, sizeof(err)));
            cast_frames |= 1u << frame;
        }
    }
    ASSERT_TRUE(dc_players_status(v,0,&p,&binding));ASSERT_EQ(p.action,FR_PLAYER_CAST);
    ASSERT_TRUE(fr_player_frame(&p,&frame));ASSERT_EQ(frame,5);
    ASSERT_TRUE(capture(v,"build/agents/beaver-player/sustained/runtime-cast.bmp",err,sizeof(err)));
    dc_gpu_world_body_t projectile;
    ASSERT_TRUE(projectile_id&&dc_level_view_body_state(v,projectile_id,&projectile,&motion,err,sizeof(err)));
    for(unsigned n=0;n<120;n++) {
        ASSERT_TRUE(dc_level_view_step(v,err,sizeof(err))&&dc_level_view_tick(v,err,sizeof(err)));
        ASSERT_TRUE(dc_players_status(v,0,&p,&binding));
        if (p.action == FR_PLAYER_CAST) {
            ASSERT_TRUE(fr_player_frame(&p,&frame) && frame < 8u);
            if (!(cast_frames & (1u << frame))) {
                ASSERT_TRUE(runtime_visual_check(v, FR_PLAYER_CAST, frame, projectile_id, err, sizeof(err)));
                cast_frames |= 1u << frame;
            }
        }
        if(n>=14) ASSERT_EQ(p.action,FR_PLAYER_IDLE);
    }
    ASSERT_EQ(cast_frames, 0xffu);
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

static void test_fire_ray_mode_uses_supplied_artwork_while_moving(void) {
    char directory[] = "build/player_runtime_ray_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    uint32_t surface = 96;
    dc_ground_profile_t ground = {.width = 1, .soil_depth = 1, .surface = &surface};
    dc_level_view_t *v = dc_level_view_create_ground(directory, 314, &ground, err, sizeof(err));
    ASSERT_TRUE(v && dc_level_view_pan_pixels(v, 0, 128) &&
                dc_level_view_wait_visible(v, 120000, err, sizeof(err)) &&
                dc_players_enable(v, err, sizeof(err)));
    const fr_fire_ray_config_t ray = {VIEW_WIDTH, 4, .4f, 1800};
    ASSERT_TRUE(dc_players_fire_ray(v, ray, err, sizeof(err)));
    for (unsigned n = 0; n < 60; n++) {
        ASSERT_TRUE(dc_level_view_step(v, err, sizeof(err)) && dc_level_view_tick(v, err, sizeof(err)));
    }
    SDL_Event move = {.key = {.type = SDL_KEYDOWN, .keysym.scancode = SDL_SCANCODE_D}};
    ASSERT_TRUE(dc_players_event(v, &move));
    dc_players_frame_time(v, .12);
    ASSERT_TRUE(dc_level_view_step(v, err, sizeof(err)) && dc_level_view_tick(v, err, sizeof(err)));
    fr_player_t player;
    fr_beaver_body_t body;
    ASSERT_TRUE(dc_players_status(v, 0, &player, &body));
    ASSERT_EQ(player.action, FR_PLAYER_WALK);
    ASSERT_TRUE(runtime_visual_check(v, FR_PLAYER_IDLE, 0u, 0u, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(v, err, sizeof(err)));
    PASS();
}

static bool walk_source_pixel(dc_level_view_t *v, uint32_t source, uint32_t *x, uint32_t *y,
                              char *err, uint32_t cap) {
    fr_player_t player;
    fr_beaver_body_t binding;
    dc_gpu_world_body_t body;
    dc_gpu_body_motion_t motion;
    dc_level_view_status_t status;
    uint32_t frame, entries;
    size_t size;
    if (!dc_players_status(v, 0, &player, &binding) || player.action != FR_PLAYER_WALK ||
        !fr_player_frame(&player, &frame) || !dc_level_view_status(v, &status) ||
        !dc_level_view_body_state(v, binding.binding.body_id, &body, &motion, err, cap) ||
        !fr_beaver_asset_size(FR_PLAYER_WALK, &size, &entries)) return false;
    uint8_t *storage = malloc(size);
    uint32_t *mapping = malloc((size_t)entries * sizeof(*mapping));
    fr_beaver_asset_t asset;
    fr_beaver_frame_layout_t layout;
    bool okay = storage && mapping &&
        fr_beaver_asset_build(FR_PLAYER_WALK, storage, size, mapping, entries, &asset, err, cap) &&
        fr_beaver_frame_layout(&asset, frame, &layout);
    bool found = false;
    if (okay) for (uint32_t texel = 0; texel < FR_BEAVER_PIXELS; ++texel) {
        if (mapping[frame * FR_BEAVER_PIXELS + texel] != source) continue;
        uint32_t column = texel % FR_BEAVER_WIDTH;
        if (player.facing < 0) column = FR_BEAVER_WIDTH - 1u - column;
        int64_t sx = (body.chunk.x - status.origin.x) * DC_CHUNK_SIDE -
            status.offset_x + body.body.x_fp / (int32_t)DC_FLUID_FULL +
            column - binding.rest_origin_x;
        int64_t sy = (body.chunk.y - status.origin.y) * DC_CHUNK_SIDE -
            status.offset_y + body.body.y_fp / (int32_t)DC_FLUID_FULL +
            texel / FR_BEAVER_WIDTH + binding.height - layout.foot_baseline;
        if (sx >= 0 && sx < VIEW_WIDTH && sy >= 0 && sy < VIEW_HEIGHT) {
            *x = (uint32_t)sx;
            *y = (uint32_t)sy;
            found = true;
        }
        break;
    }
    free(mapping);
    free(storage);
    return found;
}

static void test_wet_robe_darkening_follows_the_owner_across_walk_poses(void) {
    enum { ROBE_X = 17, ROBE_Y = 20, ROBE_SOURCE = ROBE_Y * FR_BEAVER_WIDTH + ROBE_X + 1 };
    char directory[] = "build/player_runtime_wetting_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    uint32_t surface = 96;
    dc_ground_profile_t ground = {.width = 1, .soil_depth = 1, .surface = &surface};
    dc_level_view_t *v = dc_level_view_create_ground(directory, 314, &ground, err, sizeof(err));
    ASSERT_TRUE(v && dc_level_view_pan_pixels(v, 0, -INITIAL_CHUNK_Y * DC_CHUNK_SIDE) &&
                dc_level_view_wait_visible(v, 120000, err, sizeof(err)) &&
                dc_players_enable_at(v, 80, 24, false, err, sizeof(err)));
    fr_beaver_source_t material;
    ASSERT_TRUE(fr_beaver_source(ROBE_SOURCE, &material));
    ASSERT_EQ(material.region, FR_BEAVER_ROBE);
    SDL_Event move = {.key = {.type = SDL_KEYDOWN, .keysym.scancode = SDL_SCANCODE_D}};
    ASSERT_TRUE(dc_players_event(v, &move));
    dc_players_frame_time(v, .12);
    ASSERT_TRUE(dc_players_step(v, 1.f/60.f, err, sizeof(err)) && dc_level_view_tick(v, err, sizeof(err)));
    uint32_t wet_x, wet_y, dry_color;
    ASSERT_TRUE(walk_source_pixel(v, ROBE_SOURCE, &wet_x, &wet_y, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(v, wet_x, wet_y, &dry_color, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(v, wet_x, wet_y, 0, DC_MATERIAL_WATER, err, sizeof(err)) &&
                fr_thermal_gpu_step(dc_level_view_gpu(v), .75f, err, sizeof(err)));
    fr_player_t player;
    fr_beaver_body_t body;
    ASSERT_TRUE(dc_players_status(v, 0, &player, &body));
    fr_reactive_record_t wet, turned;
    bool found = false;
    uint32_t rest_x = ROBE_X - body.rest_origin_x, rest_y = ROBE_Y - body.rest_origin_y;
    ASSERT_TRUE(fr_gpu_reactive_read_body(dc_level_view_gpu(v), body.binding.body_id, rest_x,
                                          rest_y, &wet, &found, err, sizeof(err)) && found);
    ASSERT_EQ(wet.state.material, FR_REACTIVE_BEAVER_TEXTILE);
    ASSERT_TRUE(wet.state.bound_water_fp > 0);
    move.type = SDL_KEYUP;
    ASSERT_TRUE(dc_players_event(v, &move));
    move.key.type = SDL_KEYDOWN;
    move.key.keysym.scancode = SDL_SCANCODE_A;
    ASSERT_TRUE(dc_players_event(v, &move));
    dc_players_frame_time(v, .12);
    ASSERT_TRUE(dc_players_step(v, 1.f/60.f, err, sizeof(err)) && dc_level_view_tick(v, err, sizeof(err)));
    uint32_t turned_x, turned_y, wet_color;
    ASSERT_TRUE(walk_source_pixel(v, ROBE_SOURCE, &turned_x, &turned_y, err, sizeof(err)));
    ASSERT_TRUE(turned_x != wet_x);
    ASSERT_TRUE(dc_level_view_pixel(v, turned_x, turned_y, &wet_color, err, sizeof(err)));
    ASSERT_TRUE(((wet_color >> 16u) & 255u) < ((dry_color >> 16u) & 255u));
    ASSERT_TRUE(fr_gpu_reactive_read_body(dc_level_view_gpu(v), body.binding.body_id, rest_x,
                                          rest_y, &turned, &found, err, sizeof(err)) && found);
    ASSERT_EQ(turned.state.parcel_id_low, wet.state.parcel_id_low);
    ASSERT_EQ(turned.state.parcel_id_high, wet.state.parcel_id_high);
    ASSERT_EQ(turned.state.material, wet.state.material);
    ASSERT_EQ(turned.state.bound_water_fp, wet.state.bound_water_fp);
    ASSERT_TRUE(dc_level_view_destroy(v, err, sizeof(err)));
    PASS();
}

static void test_heat_probe_targets_the_visible_robe_in_a_two_player_scene(void) {
    enum { ROBE_X = 17, ROBE_Y = 20, ROBE_SOURCE = ROBE_Y * FR_BEAVER_WIDTH + ROBE_X + 1 };
    char directory[] = "build/player_runtime_heating_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    uint32_t surface = 96;
    dc_ground_profile_t ground = {.width = 1, .soil_depth = 1, .surface = &surface};
    dc_level_view_t *v = dc_level_view_create_ground(directory, 314, &ground, err, sizeof(err));
    ASSERT_TRUE(v && dc_level_view_pan_pixels(v, 0, -INITIAL_CHUNK_Y * DC_CHUNK_SIDE) &&
                dc_level_view_wait_visible(v, 120000, err, sizeof(err)) &&
                dc_players_enable_at(v, 80, 24, false, err, sizeof(err)) &&
                dc_players_join(v, 42, err, sizeof(err)));
    SDL_Event move = {.key = {.type = SDL_KEYDOWN, .keysym.scancode = SDL_SCANCODE_D}};
    ASSERT_TRUE(dc_players_event(v, &move));
    dc_players_frame_time(v, .12);
    ASSERT_TRUE(dc_players_step(v, 1.f/60.f, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_heat_overlay(v, 1) && dc_level_view_tick(v, err, sizeof(err)));
    uint32_t x, y, cold_color, hot_color;
    ASSERT_TRUE(walk_source_pixel(v, ROBE_SOURCE, &x, &y, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(v, x, y, &cold_color, err, sizeof(err)));
    dc_gpu_t *gpu = dc_level_view_gpu(v);
    fr_player_t player;
    fr_beaver_body_t bodies[2];
    fr_reactive_record_t before[2];
    for (uint32_t seat=0; seat<2; ++seat) {
        bool found = false;
        ASSERT_TRUE(dc_players_status(v, seat, &player, bodies + seat));
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, bodies[seat].binding.body_id,
            ROBE_X - bodies[seat].rest_origin_x, ROBE_Y - bodies[seat].rest_origin_y,
            before + seat, &found, err, sizeof(err)) && found);
    }
    ASSERT_TRUE(dc_level_view_heat_probe(v, x, y, true));
    ASSERT_TRUE(fr_thermal_gpu_step(gpu, .75f, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_heat_probe(v, x, y, false));
    ASSERT_TRUE(dc_level_view_tick(v, err, sizeof(err)));
    for (uint32_t seat=0; seat<2; ++seat) {
        fr_reactive_record_t after;
        bool found = false;
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, bodies[seat].binding.body_id,
            ROBE_X - bodies[seat].rest_origin_x, ROBE_Y - bodies[seat].rest_origin_y,
            &after, &found, err, sizeof(err)) && found);
        double heated = fr_reactive_state_energy(&after.state) - fr_reactive_state_energy(&before[seat].state);
        ASSERT_EQ(after.state.parcel_id_low, before[seat].state.parcel_id_low);
        ASSERT_EQ(after.state.parcel_id_high, before[seat].state.parcel_id_high);
        ASSERT_TRUE(seat ? fabs(heated) < .001 : heated > .05);
    }
    ASSERT_TRUE(dc_level_view_pixel(v, x, y, &hot_color, err, sizeof(err)));
    ASSERT_TRUE(((hot_color >> 8u) & 255u) > ((cold_color >> 8u) & 255u));
    ASSERT_TRUE(dc_level_view_destroy(v, err, sizeof(err)));
    PASS();
}

static void test_visible_robe_and_hat_singe_and_stay_dark_after_reversal(void) {
    enum { PARTS = 2 };
    const uint32_t rest_canvas[PARTS][2] = {{14u, 23u}, {32u, 9u}};
    const int32_t air_side[PARTS] = {1, -1};
    const fr_beaver_region_t regions[PARTS] = {FR_BEAVER_ROBE, FR_BEAVER_HAT};
    char directory[] = "build/player_runtime_singe_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    uint32_t surface = 96;
    dc_ground_profile_t ground = {.width = 1, .soil_depth = 1, .surface = &surface};
    dc_level_view_t *v = dc_level_view_create_ground(directory, 314, &ground, err, sizeof(err));
    ASSERT_TRUE(v && dc_level_view_pan_pixels(v, 0, -INITIAL_CHUNK_Y * DC_CHUNK_SIDE) &&
                dc_level_view_wait_visible(v, 120000, err, sizeof(err)) &&
                dc_players_enable_at(v, 80, 24, false, err, sizeof(err)) &&
                dc_players_join(v, 42, err, sizeof(err)));
    dc_gpu_t *gpu = dc_level_view_gpu(v);
    ASSERT_TRUE(dc_level_view_configure_atmosphere(v, err, sizeof(err)) &&
                fr_combustion_gpu_enable(gpu, err, sizeof(err)) &&
                dc_level_view_heat_overlay(v, 0));
    SDL_Event move = {.key = {.type = SDL_KEYDOWN, .keysym.scancode = SDL_SCANCODE_A}};
    ASSERT_TRUE(dc_players_event(v, &move));
    dc_players_frame_time(v, .12);
    ASSERT_TRUE(dc_players_step(v, 1.f/60.f, err, sizeof(err)) && dc_level_view_tick(v, err, sizeof(err)) &&
                fr_gas_gpu_refresh_geometry(gpu, err, sizeof(err)));

    dc_level_view_status_t status;
    fr_player_t player;
    fr_beaver_body_t bodies[2];
    ASSERT_TRUE(dc_level_view_status(v, &status) &&
                dc_players_status(v, 0, &player, &bodies[0]) &&
                dc_players_status(v, 1, &player, &bodies[1]));
    fr_gas_state_t *gas = calloc(DC_CHUNK_CELLS, sizeof(*gas));
    ASSERT_TRUE(gas);
    uint32_t sources[PARTS], original_x[PARTS], original_rgb[PARTS];
    fr_reactive_record_t before[PARTS], control[PARTS], burned[PARTS];
    for (uint32_t part = 0; part < PARTS; ++part) {
        uint32_t sx = rest_canvas[part][0], sy = rest_canvas[part][1];
        sources[part] = sy * FR_BEAVER_WIDTH + sx + 1u;
        fr_beaver_source_t source;
        ASSERT_TRUE(fr_beaver_source(sources[part], &source) && source.region == regions[part]);
        uint32_t y;
        ASSERT_TRUE(walk_source_pixel(v, sources[part], &original_x[part], &y, err, sizeof(err)) &&
                    original_x[part] > 0u &&
                    dc_level_view_pixel(v, original_x[part], y, &original_rgb[part], err, sizeof(err)));
        const uint32_t air_x = (uint32_t)((int32_t)original_x[part] + air_side[part]) + status.offset_x;
        const uint32_t air_y = y + status.offset_y;
        dc_chunk_coord_t coord = {status.origin.x + air_x / DC_CHUNK_SIDE,
                                  status.origin.y + air_y / DC_CHUNK_SIDE};
        uint32_t slot = dc_gpu_find_active_chunk(gpu, coord);
        const uint32_t cell = air_y % DC_CHUNK_SIDE * DC_CHUNK_SIDE + air_x % DC_CHUNK_SIDE;
        ASSERT_TRUE(slot != UINT32_MAX && fr_gas_gpu_read_chunk(gpu, slot, gas, DC_CHUNK_CELLS, err, sizeof(err)));
        ASSERT_TRUE(gas[cell].species[FR_GAS_OXYGEN] > 0.);
        bool found = false;
        const uint32_t x = sx - bodies[0].rest_origin_x, py = sy - bodies[0].rest_origin_y;
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, bodies[0].binding.body_id, x, py,
                                              &before[part], &found, err, sizeof(err)) && found);
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, bodies[1].binding.body_id,
                                              sx - bodies[1].rest_origin_x, sy - bodies[1].rest_origin_y,
                                              &control[part], &found, err, sizeof(err)) && found);
        fr_reactive_state_t hot = before[part].state;
        hot.ignition_threshold = .001f;
        const double heat = fr_reactive_state_energy_at_temperature(&hot, 1150.f) - fr_reactive_state_energy(&hot);
        ASSERT_TRUE(fr_reactive_state_add_heat(&hot, heat, err, sizeof(err)) &&
                    fr_gpu_reactive_seed_body(gpu, bodies[0].binding.body_id, x, py, &hot, err, sizeof(err)));
    }
    free(gas);
    for (uint32_t step = 0; step < 4u; ++step)
        ASSERT_TRUE(fr_combustion_gpu_step(gpu, .25f, err, sizeof(err)));
    for (uint32_t part = 0; part < PARTS; ++part) {
        bool found = false;
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, bodies[0].binding.body_id,
            before[part].owner.key[2], before[part].owner.key[3], &burned[part], &found, err, sizeof(err)) && found);
        printf("Walking %s gained %u char\n", part == 0u ? "robe" : "hat", burned[part].state.char_mass_fp);
        ASSERT_TRUE(burned[part].state.fuel_mass_fp < before[part].state.fuel_mass_fp &&
                    burned[part].state.char_mass_fp > before[part].state.char_mass_fp);
    }
    move.type = SDL_KEYUP;
    ASSERT_TRUE(dc_players_event(v, &move));
    move.key.type = SDL_KEYDOWN;
    move.key.keysym.scancode = SDL_SCANCODE_D;
    ASSERT_TRUE(dc_players_event(v, &move));
    dc_players_frame_time(v, .12);
    ASSERT_TRUE(dc_players_step(v, 1.f/60.f, err, sizeof(err)) && dc_level_view_tick(v, err, sizeof(err)));
    for (uint32_t part = 0; part < PARTS; ++part) {
        uint32_t x, y, rgb;
        ASSERT_TRUE(walk_source_pixel(v, sources[part], &x, &y, err, sizeof(err)) && x != original_x[part] &&
                    dc_level_view_pixel(v, x, y, &rgb, err, sizeof(err)));
        ASSERT_TRUE(((rgb >> 8u) & 255u) < ((original_rgb[part] >> 8u) & 255u));
        bool found = false;
        fr_reactive_record_t turned, other;
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, bodies[0].binding.body_id,
            burned[part].owner.key[2], burned[part].owner.key[3], &turned, &found, err, sizeof(err)) && found);
        ASSERT_EQ(turned.state.parcel_id_low, before[part].state.parcel_id_low);
        ASSERT_EQ(turned.state.parcel_id_high, before[part].state.parcel_id_high);
        ASSERT_EQ(turned.state.char_mass_fp, burned[part].state.char_mass_fp);
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, bodies[1].binding.body_id,
            control[part].owner.key[2], control[part].owner.key[3], &other, &found, err, sizeof(err)) && found);
        ASSERT_EQ(other.state.parcel_id_low, control[part].state.parcel_id_low);
        ASSERT_EQ(other.state.fuel_mass_fp, control[part].state.fuel_mass_fp);
        ASSERT_EQ(other.state.char_mass_fp, control[part].state.char_mass_fp);
    }
    ASSERT_TRUE(dc_level_view_destroy(v, err, sizeof(err)));
    PASS();
}

int main(void) {
    RUN(test_enabling_player_admits_only_the_player_body);
    RUN(test_editor_camera_holds_its_manual_position);
    RUN(test_real_app_keyboard_drives_body_and_animation);
    RUN(test_fire_ray_mode_uses_supplied_artwork_while_moving);
    RUN(test_wet_robe_darkening_follows_the_owner_across_walk_poses);
    RUN(test_heat_probe_targets_the_visible_robe_in_a_two_player_scene);
    RUN(test_visible_robe_and_hat_singe_and_stay_dark_after_reversal);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
