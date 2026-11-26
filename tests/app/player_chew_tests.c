#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/material_density.h"
#include "../../src/app/player_session.h"
#include "../../src/app/view_config.h"

static int g_pass, g_fail;
static char err[256];
static dc_level_view_t *view;

#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();cleanup();} while (0)
#define ASSERT_TRUE(e) do {if (!(e)) {printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

static void cleanup(void) {
    if (view && !dc_level_view_destroy(view, err, sizeof(err))) {
        printf("FAIL closing level: %s\n", err);
        g_fail++;
    }
    view = NULL;
}

static fr_player_point_t world_point(const dc_level_view_status_t *camera,
                                     uint32_t x, uint32_t y) {
    dc_chunk_coord_t chunk;
    uint32_t lx, ly;
    dc_cell_address(x + camera->offset_x, y + camera->offset_y, &chunk, &lx, &ly);
    return (fr_player_point_t){{camera->origin.x + chunk.x,
                                camera->origin.y + chunk.y}, (float)lx, (float)ly};
}

static uint16_t get_material(fr_player_point_t point) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return UINT16_MAX;
    bool okay = dc_level_view_chunk(view, point.chunk, chunk, err, sizeof(err));
    uint32_t local = (uint32_t)point.y * DC_CHUNK_SIDE + (uint32_t)point.x;
    uint16_t material = okay ? chunk->cells[local].material : UINT16_MAX;
    free(chunk);
    return material;
}

static bool action_step(void) {
    /* Hold physics fixed so target loss in this fixture depends on input or cuts. */
    dc_players_frame_time(view, .15);
    return dc_players_step(view, 1.f / 60.f, err, sizeof(err));
}

static void test_chewing_obeys_live_player_reach_and_drives_animation(void) {
    char directory[] = "build/player_chew_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    uint32_t surface = 96;
    dc_ground_profile_t ground = {.width = 1, .soil_depth = 1, .surface = &surface};
    view = dc_level_view_create_ground(directory, 314, &ground, err, sizeof(err));
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 0, -INITIAL_CHUNK_Y * 64));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_players_enable_at(view, 64, 40, false, err, sizeof(err)));

    dc_level_view_status_t camera;
    ASSERT_TRUE(dc_level_view_status(view, &camera));
    ASSERT_TRUE(dc_level_view_paint(view, 73u, 40u, 0u, DC_MATERIAL_WOOD,
                                    err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 137u, 40u, 0u, DC_MATERIAL_WOOD,
                                    err, sizeof(err)));
    const fr_player_point_t nearby_cell = world_point(&camera, 73u, 40u);
    const fr_player_point_t distant_cell = world_point(&camera, 137u, 40u);
    ASSERT_EQ(get_material(nearby_cell), DC_MATERIAL_WOOD);
    ASSERT_EQ(get_material(distant_cell), DC_MATERIAL_WOOD);

    SDL_Event key = {.type = SDL_KEYDOWN};
    key.key.keysym.scancode = SDL_SCANCODE_C;
    ASSERT_TRUE(dc_players_aim_override(view, 0, &distant_cell, err, sizeof(err)));
    ASSERT_TRUE(dc_players_event(view, &key));
    for (unsigned tick = 0; tick < 5; ++tick) ASSERT_TRUE(action_step());

    fr_player_inventory_t inv;
    fr_player_t player;
    fr_beaver_body_t body;
    ASSERT_TRUE(dc_players_inventory(view, 0, &inv));
    ASSERT_EQ(fr_player_inventory_mass(&inv, DC_ITEM_WOOD), 0u);
    ASSERT_EQ(get_material(distant_cell), DC_MATERIAL_WOOD);
    ASSERT_TRUE(dc_players_status(view, 0, &player, &body));
    ASSERT_INT_EQ(player.action, FR_PLAYER_IDLE);

    ASSERT_TRUE(dc_players_aim_override(view, 0, &nearby_cell, err, sizeof(err)));
    ASSERT_TRUE(action_step());
    ASSERT_TRUE(dc_players_status(view, 0, &player, &body));
    ASSERT_INT_EQ(player.action, FR_PLAYER_CHEW);
    ASSERT_TRUE(!player.chew_recovering);
    ASSERT_EQ(get_material(nearby_cell), DC_MATERIAL_WOOD);

    key.type = SDL_KEYUP;
    ASSERT_TRUE(dc_players_event(view, &key));
    ASSERT_TRUE(action_step());
    ASSERT_TRUE(dc_players_status(view, 0, &player, &body));
    ASSERT_INT_EQ(player.action, FR_PLAYER_IDLE);
    ASSERT_EQ(get_material(nearby_cell), DC_MATERIAL_WOOD);

    key.type = SDL_KEYDOWN;
    ASSERT_TRUE(dc_players_event(view, &key));
    for (unsigned tick = 0; tick < 4; ++tick) ASSERT_TRUE(action_step());
    ASSERT_TRUE(dc_players_inventory(view, 0, &inv));
    ASSERT_EQ(fr_player_inventory_mass(&inv, DC_ITEM_WOOD),
              (uint64_t)lround(DC_DENSITY_WOOD * DC_FLUID_FULL));
    ASSERT_EQ(get_material(nearby_cell), DC_MATERIAL_AIR);
    ASSERT_EQ(get_material(distant_cell), DC_MATERIAL_WOOD);
    PASS();
}

int main(void) {
    printf("=== Player Chew App Tests ===\n");
    RUN(test_chewing_obeys_live_player_reach_and_drives_animation);
    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
