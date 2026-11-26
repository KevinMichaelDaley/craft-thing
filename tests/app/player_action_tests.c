#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/material_density.h"
#include "ferrum/fire/gpu.h"
#include "../../src/app/player_session.h"
#include "../../src/app/view_config.h"

static int g_pass, g_fail;
static char err[256];
static dc_level_view_t *view;

static void close_fixture(void);
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();close_fixture();} while (0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

static bool open_fixture(void) {
    char directory[] = "build/player_actions_XXXXXX";
    uint32_t surface = 96;
    dc_ground_profile_t ground = {.width = 1, .soil_depth = 1, .surface = &surface};
    err[0] = 0;
    if (!mkdtemp(directory)) return false;
    view = dc_level_view_create_ground(directory, 314, &ground, err, sizeof(err));
    return view && dc_level_view_pan_pixels(view, 0, -INITIAL_CHUNK_Y * 64) &&
        dc_level_view_wait_visible(view, 120000, err, sizeof(err)) &&
        dc_players_enable_at(view, 64, 40, false, err, sizeof(err));
}

static void close_fixture(void) {
    if (view && !dc_level_view_destroy(view, err, sizeof(err))) {
        printf("FAIL closing fixture: %s\n", err);
        g_fail++;
    }
    view = NULL;
}

static bool key(uint32_t type, SDL_Scancode code) {
    SDL_Event e = {.type = type};
    e.key.keysym.scancode = code;
    return dc_players_event(view, &e);
}

static bool tap(SDL_Scancode code) {
    return key(SDL_KEYDOWN, code) && key(SDL_KEYUP, code);
}

static bool pad(uint32_t player, uint8_t button) {
    SDL_Event e = {.type = SDL_CONTROLLERBUTTONDOWN};
    e.cbutton.which = (int32_t)player;
    e.cbutton.button = button;
    return dc_players_event(view, &e);
}

static bool step(float seconds) {
    return dc_players_step(view, seconds, err, sizeof(err));
}

static bool aim(uint32_t player, uint32_t x, uint32_t y) {
    dc_chunk_coord_t chunk;
    uint32_t lx, ly;
    dc_cell_address(x, y, &chunk, &lx, &ly);
    fr_player_point_t point = {chunk, (float)lx, (float)ly};
    return dc_players_aim_override(view, player, &point, err, sizeof(err));
}

static bool read_cell(uint32_t x, uint32_t y, dc_cell_t *out) {
    dc_chunk_t *chunk = malloc(sizeof(*chunk));
    if (!chunk) return false;
    dc_chunk_coord_t coord;
    uint32_t lx, ly;
    dc_cell_address(x, y, &coord, &lx, &ly);
    bool found = dc_level_view_chunk(view, coord, chunk, err, sizeof(err));
    if (found) *out = chunk->cells[ly * DC_CHUNK_SIDE + lx];
    free(chunk);
    return found;
}

static bool paint(uint32_t x, uint32_t y, uint16_t material) {
    return dc_level_view_paint(view, x, y, 0u, material, err, sizeof(err));
}

static void test_selected_cast_latches_aim_and_fires_once_at_frame_six(void) {
    ASSERT_TRUE(open_fixture());
    dc_cell_t first, second;
    fr_player_t player;
    fr_beaver_body_t body;
    ASSERT_TRUE(aim(0, 170, 42) && tap(SDL_SCANCODE_2) && step(.125f));
    ASSERT_TRUE(dc_players_status(view, 0, &player, &body));
    ASSERT_INT_EQ(player.action, FR_PLAYER_IDLE);
    ASSERT_TRUE(read_cell(170, 42, &first));
    ASSERT_EQ(first.fluid_mass, 0u);

    ASSERT_TRUE(key(SDL_KEYDOWN, SDL_SCANCODE_Q) && step(.25f));
    ASSERT_TRUE(aim(0, 202, 42) && tap(SDL_SCANCODE_1) && step(.125f));
    ASSERT_TRUE(read_cell(170, 42, &first));
    ASSERT_EQ(first.fluid_mass, 0u);
    ASSERT_TRUE(step(.03125f));
    ASSERT_TRUE(dc_players_status(view, 0, &player, &body));
    uint32_t frame;
    ASSERT_TRUE(fr_player_frame(&player, &frame));
    ASSERT_EQ(frame, FR_PLAYER_CAST_EVENT_FRAME);
    ASSERT_TRUE(read_cell(170, 42, &first) && read_cell(202, 42, &second));
    ASSERT_EQ(first.fluid_mass, DC_FLUID_FULL);
    ASSERT_EQ(second.fluid_mass, 0u);

    ASSERT_TRUE(paint(170, 42, DC_MATERIAL_AIR));
    ASSERT_TRUE(step(.25f) && step(.25f));
    ASSERT_TRUE(dc_players_status(view, 0, &player, &body));
    ASSERT_INT_EQ(player.action, FR_PLAYER_IDLE);
    ASSERT_TRUE(read_cell(170, 42, &first) && read_cell(202, 42, &second));
    ASSERT_EQ(first.fluid_mass, 0u);
    ASSERT_EQ(second.fluid_mass, 0u);
    PASS();
}

static void test_keyboard_fire_bolt_leaves_the_live_beaver_as_a_burning_body(void) {
    ASSERT_TRUE(open_fixture());
    dc_gpu_t *gpu = dc_level_view_gpu(view);
    fr_player_t player;
    fr_beaver_body_t beaver;
    ASSERT_TRUE(dc_players_status(view, 0, &player, &beaver));
    dc_gpu_world_body_t caster, projectile;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, beaver.binding.body_id, &caster,
                                       err, sizeof(err)));
    uint32_t expected_id = dc_gpu_next_body_id(gpu);
    ASSERT_TRUE(expected_id && aim(0, 175, 56));
    ASSERT_TRUE(tap(SDL_SCANCODE_Q) && step(.25f) && step(.15625f));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, expected_id, &projectile, err, sizeof(err)));
    int64_t delta = (projectile.chunk.x - caster.chunk.x) * DC_CHUNK_SIDE * DC_FLUID_FULL +
                    projectile.body.x_fp - caster.body.x_fp;
    ASSERT_TRUE(delta >= (int64_t)caster.body.width * DC_FLUID_FULL);
    ASSERT_TRUE(projectile.body.vx_fp > 3 * (int32_t)DC_FLUID_FULL);
    fr_reactive_record_t parcel;
    bool found;
    ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, expected_id, 1, 1, &parcel, &found,
                                          err, sizeof(err)) && found);
    ASSERT_EQ(parcel.owner.key[0], expected_id);
    ASSERT_EQ(parcel.state.material, DC_MATERIAL_COAL);
    ASSERT_TRUE((parcel.state.flags & FR_REACTIVE_FLAMING) != 0u);
    uint32_t next = dc_gpu_next_body_id(gpu);
    ASSERT_TRUE(step(.25f) && step(.25f));
    ASSERT_EQ(dc_gpu_next_body_id(gpu), next);
    PASS();
}

static void test_controller_selection_and_cast_belong_to_its_seat(void) {
    ASSERT_TRUE(open_fixture());
    ASSERT_TRUE(dc_players_join(view, 42, err, sizeof(err)));
    ASSERT_TRUE(aim(0, 170, 42) && aim(1, 211, 42));
    bool selected = pad(42, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    ASSERT_TRUE(tap(SDL_SCANCODE_3) && step(.125f));
    ASSERT_TRUE(selected);
    ASSERT_TRUE(key(SDL_KEYDOWN, SDL_SCANCODE_Q) && pad(42, SDL_CONTROLLER_BUTTON_X));
    ASSERT_TRUE(step(.25f) && step(.15625f));
    fr_player_t keyboard, controller;
    fr_beaver_body_t body;
    ASSERT_TRUE(dc_players_status(view, 0, &keyboard, &body));
    ASSERT_TRUE(dc_players_status(view, 1, &controller, &body));
    ASSERT_INT_EQ(keyboard.action, FR_PLAYER_IDLE);
    ASSERT_INT_EQ(controller.action, FR_PLAYER_CAST);
    dc_cell_t first, second;
    ASSERT_TRUE(read_cell(170, 42, &first) && read_cell(211, 42, &second));
    ASSERT_EQ(first.fluid_mass, 0u);
    ASSERT_EQ(second.fluid_mass, DC_FLUID_FULL);
    PASS();
}

static void test_empty_inventory_places_neither_terrain_nor_body(void) {
    ASSERT_TRUE(open_fixture());
    dc_gpu_t *gpu = dc_level_view_gpu(view);
    dc_gpu_extraction_stats_t before, after;
    ASSERT_TRUE(dc_gpu_read_extraction_stats(gpu, &before));
    ASSERT_TRUE(aim(0, 170, 42) && tap(SDL_SCANCODE_4) && step(.125f));
    ASSERT_TRUE(tap(SDL_SCANCODE_R) && step(.125f));
    ASSERT_TRUE(tap(SDL_SCANCODE_B) && step(.125f));
    ASSERT_TRUE(dc_gpu_read_extraction_stats(gpu, &after));
    ASSERT_EQ(after.owned_bodies, before.owned_bodies);
    dc_cell_t cell;
    ASSERT_TRUE(read_cell(170, 42, &cell));
    ASSERT_EQ(cell.material, DC_MATERIAL_AIR);
    PASS();
}

static void test_paid_placement_is_typed_and_rejects_occupied_cells(void) {
    ASSERT_TRUE(open_fixture());
    ASSERT_TRUE(key(SDL_KEYDOWN, SDL_SCANCODE_C));
    const uint32_t harvested = 34;
    for (uint32_t n = 0; n < harvested; ++n) {
        uint32_t x = 68 + n % 6, y = 40 + n / 6;
        ASSERT_TRUE(paint(x, y, DC_MATERIAL_WOOD) && aim(0, x, y));
        ASSERT_TRUE(step(.25f) && step(.25f) && step(.25f));
    }
    ASSERT_TRUE(key(SDL_KEYUP, SDL_SCANCODE_C));
    fr_player_inventory_t inventory;
    uint64_t cell_mass = (uint64_t)lround(DC_DENSITY_WOOD * DC_FLUID_FULL);
    uint64_t block_mass = (uint64_t)lround(16. * DC_DENSITY_WOOD * DC_FLUID_FULL);
    ASSERT_TRUE(dc_players_inventory(view, 0, &inventory));
    ASSERT_EQ(fr_player_inventory_mass(&inventory, DC_ITEM_WOOD), harvested * cell_mass);
    dc_cell_t cell;
    ASSERT_TRUE(tap(SDL_SCANCODE_3) && aim(0, 171, 44));
    ASSERT_TRUE(tap(SDL_SCANCODE_R) && step(.125f));
    ASSERT_TRUE(read_cell(171, 44, &cell));
    ASSERT_EQ(cell.material, DC_MATERIAL_AIR);
    ASSERT_TRUE(dc_players_inventory(view, 0, &inventory));
    ASSERT_EQ(fr_player_inventory_mass(&inventory, DC_ITEM_WOOD), harvested * cell_mass);

    ASSERT_TRUE(tap(SDL_SCANCODE_4) && aim(0, 171, 44));
    ASSERT_TRUE(tap(SDL_SCANCODE_R) && step(.125f));
    ASSERT_TRUE(read_cell(171, 44, &cell));
    ASSERT_EQ(cell.material, DC_MATERIAL_WOOD);
    ASSERT_TRUE((cell.flags & DC_CELL_LOADED_TERRAIN) != 0u);
    ASSERT_TRUE(dc_players_inventory(view, 0, &inventory));
    const uint64_t after_terrain = (harvested - 1u) * cell_mass;
    ASSERT_EQ(fr_player_inventory_mass(&inventory, DC_ITEM_WOOD), after_terrain);
    ASSERT_TRUE(tap(SDL_SCANCODE_R) && step(.125f));
    ASSERT_TRUE(dc_players_inventory(view, 0, &inventory));
    ASSERT_EQ(fr_player_inventory_mass(&inventory, DC_ITEM_WOOD), after_terrain);

    dc_gpu_t *gpu = dc_level_view_gpu(view);
    uint32_t expected_id = dc_gpu_next_body_id(gpu);
    ASSERT_TRUE(expected_id);
    ASSERT_TRUE(paint(192, 43, DC_MATERIAL_STONE) && aim(0, 191, 42));
    ASSERT_TRUE(tap(SDL_SCANCODE_B) && step(.125f));
    ASSERT_EQ(dc_gpu_next_body_id(gpu), expected_id);
    ASSERT_TRUE(dc_players_inventory(view, 0, &inventory));
    ASSERT_EQ(fr_player_inventory_mass(&inventory, DC_ITEM_WOOD), after_terrain);

    ASSERT_TRUE(paint(192, 43, DC_MATERIAL_AIR));
    ASSERT_TRUE(tap(SDL_SCANCODE_B) && step(.125f));
    dc_gpu_world_body_t block;
    dc_gpu_body_shape_t shape;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, expected_id, &block, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_shape(gpu, expected_id, &shape, err, sizeof(err)));
    ASSERT_EQ(shape.material, DC_GPU_BODY_WOOD);
    ASSERT_EQ(shape.count, 4u);
    ASSERT_EQ(block.chunk.x, 2);
    ASSERT_EQ(block.body.x_fp, 63 * DC_FLUID_FULL);
    ASSERT_EQ(block.body.width, 4u);
    ASSERT_EQ(block.body.height, 4u);
    ASSERT_TRUE(dc_players_inventory(view, 0, &inventory));
    ASSERT_EQ(fr_player_inventory_mass(&inventory, DC_ITEM_WOOD), after_terrain - block_mass);

    uint32_t other_id = dc_gpu_next_body_id(gpu);
    ASSERT_TRUE(other_id && other_id != expected_id);
    ASSERT_TRUE(tap(SDL_SCANCODE_B) && step(.125f));
    ASSERT_EQ(dc_gpu_next_body_id(gpu), other_id);
    ASSERT_TRUE(dc_players_inventory(view, 0, &inventory));
    ASSERT_EQ(fr_player_inventory_mass(&inventory, DC_ITEM_WOOD), after_terrain - block_mass);
    ASSERT_TRUE(aim(0, 192, 43) && tap(SDL_SCANCODE_R) && step(.125f));
    ASSERT_TRUE(read_cell(192, 43, &cell));
    ASSERT_EQ(cell.material, DC_MATERIAL_AIR);
    ASSERT_TRUE(dc_players_inventory(view, 0, &inventory));
    ASSERT_EQ(fr_player_inventory_mass(&inventory, DC_ITEM_WOOD), after_terrain - block_mass);

    ASSERT_TRUE(aim(0, 223, 42) && tap(SDL_SCANCODE_B) && step(.125f));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, other_id, &block, err, sizeof(err)));
    ASSERT_EQ(block.chunk.x, 3);
    ASSERT_EQ(block.body.x_fp, 31 * DC_FLUID_FULL);
    ASSERT_TRUE(dc_gpu_read_body_shape(gpu, other_id, &shape, err, sizeof(err)));
    ASSERT_EQ(shape.material, DC_GPU_BODY_WOOD);
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, expected_id, &block, err, sizeof(err)));
    ASSERT_EQ(block.chunk.x, 2);
    ASSERT_EQ(block.body.x_fp, 63 * DC_FLUID_FULL);
    ASSERT_TRUE(dc_players_inventory(view, 0, &inventory));
    ASSERT_EQ(fr_player_inventory_mass(&inventory, DC_ITEM_WOOD), after_terrain - 2 * block_mass);

    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)) &&
                dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(read_cell(171, 44, &cell));
    ASSERT_EQ(cell.material, DC_MATERIAL_WOOD);
    ASSERT_TRUE((cell.flags & DC_CELL_LOADED_TERRAIN) != 0u);
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_level_view_body_state(view, expected_id, &block, &motion,
                                        err, sizeof(err)));
    ASSERT_TRUE(fabs(motion.mass - 16. * DC_DENSITY_WOOD) < .001);
    PASS();
}

int main(void) {
    RUN(test_selected_cast_latches_aim_and_fires_once_at_frame_six);
    RUN(test_keyboard_fire_bolt_leaves_the_live_beaver_as_a_burning_body);
    RUN(test_controller_selection_and_cast_belong_to_its_seat);
    RUN(test_empty_inventory_places_neither_terrain_nor_body);
    RUN(test_paid_placement_is_typed_and_rejects_occupied_cells);
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
