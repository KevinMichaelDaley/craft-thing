#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dungeoncraft/material_density.h"
#include "ferrum/physics/authored_rigid_gpu.h"
#include "../../src/app/player_session.h"
#include "../../src/app/prop_scene.h"
#include "../../src/app/view_config.h"
#include "../../src/vulkan/runtime_body_gpu.h"

static int g_pass, g_fail;
static char err[256];
static dc_level_view_t *view;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fflush(stdout); fn(); close_world(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { \
    printf("FAIL %s:%d: %s [%s]\n", __FILE__, __LINE__, #e, err); \
    g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

static bool close_world(void) {
    bool okay = dc_level_view_destroy(view, err, sizeof(err));
    view = NULL;
    if (!okay) {
        printf("FAIL closing spawn world: %s\n", err);
        g_fail++;
    }
    return okay;
}

static bool launch_play(const char *directory) {
    char capture[256];
    int length = snprintf(capture, sizeof(capture), "%s/clearing.bmp", directory);
    if (length < 0 || (size_t)length >= sizeof(capture)) return false;
    pid_t child = fork();
    if (child < 0) return false;
    if (!child) {
        execl("./build/dungeoncraft_quarter_native", "dungeoncraft_quarter_native",
              "--play", "--world-dir", directory, "--benchmark-frames", "1",
              "--benchmark-capture", capture, (char *)NULL);
        _exit(127);
    }
    int status;
    pid_t finished;
    do finished = waitpid(child, &status, 0);
    while (finished < 0 && errno == EINTR);
    return finished == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool open_world(const char *directory) {
    uint32_t surface = 96;
    dc_ground_profile_t ground = {.width = 1, .soil_depth = 1, .surface = &surface};
    view = dc_level_view_create_ground(directory, 314u, &ground, err, sizeof(err));
    return view && dc_level_view_restored(view) &&
           dc_level_view_pan_pixels(view, -(int32_t)DC_CHUNK_SIDE,
                                     -INITIAL_CHUNK_Y * (int32_t)DC_CHUNK_SIDE) &&
           dc_level_view_wait_visible(view, 120000u, err, sizeof(err));
}

static bool read_cell(int32_t x, int32_t y, dc_cell_t *cell) {
    dc_chunk_coord_t coord;
    uint32_t lx, ly;
    dc_cell_address(x, y, &coord, &lx, &ly);
    dc_chunk_t *chunk = malloc(sizeof(*chunk));
    if (!chunk) return false;
    bool okay = dc_level_view_chunk(view, coord, chunk, err, sizeof(err));
    if (okay) *cell = chunk->cells[ly * DC_CHUNK_SIDE + lx];
    free(chunk);
    return okay;
}

static bool rocks(uint32_t ids[3], uint32_t *owned) {
    const uint64_t transaction = UINT64_C(0x504c415953544f4e);
    const uint32_t rock_sizes[] = {14u, 13u, 19u};
    dc_gpu_t *gpu = dc_level_view_gpu(view);
    dc_gpu_extraction_stats_t stats;
    if (!dc_gpu_read_extraction_stats(gpu, &stats)) return false;
    *owned = stats.owned_bodies;
    for (uint32_t slot = 0; slot < dc_gpu_body_capacity(gpu); ++slot) {
        const dc_body_storage_owner_t *owner = dc_gpu_body_owner(gpu, slot);
        if (owner->state != DC_BODY_OWNER_LIVE ||
            owner->source_transaction.low != (uint32_t)transaction ||
            owner->source_transaction.high != (uint32_t)(transaction >> 32)) continue;
        if (!owner->source_object || owner->source_object > 3u ||
            ids[owner->source_object - 1u] || !owner->generation) return false;
        uint32_t index = owner->source_object - 1u;
        dc_gpu_body_pixel_t pixels[19];
        uint32_t count;
        if (!dc_gpu_read_pixel_shape(gpu, owner->runtime_id, pixels, 19u, &count,
                                     err, sizeof(err)) || count != rock_sizes[index]) return false;
        for (uint32_t n = 0; n < count; ++n)
            if (pixels[n].material != DC_GPU_BODY_STONE) return false;
        ids[index] = owner->runtime_id;
    }
    return true;
}

static void test_play_loads_authored_trees_and_preserves_harvest_and_rocks(void) {
    char directory[] = "build/play_spawn_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    ASSERT_TRUE(launch_play(directory) && open_world(directory));
    dc_cell_t cell;
    ASSERT_TRUE(read_cell(-12, 74, &cell));
    ASSERT_EQ(cell.material, DC_MATERIAL_WOOD);
    ASSERT_TRUE((cell.flags & DC_CELL_LOADED_TERRAIN) != 0u);
    ASSERT_TRUE(read_cell(-20, 96, &cell) && cell.material == DC_MATERIAL_WOOD);
    ASSERT_TRUE(read_cell(-51, 56, &cell) && cell.material == DC_MATERIAL_LEAVES);
    ASSERT_TRUE(read_cell(148, 74, &cell) && cell.material == DC_MATERIAL_WOOD);

    uint32_t original[3] = {0}, owned;
    ASSERT_TRUE(rocks(original, &owned));
    ASSERT_TRUE(original[0] && original[1] && original[2]);
    ASSERT_EQ(owned, 4u);
    dc_gpu_world_body_t west;
    dc_gpu_t *gpu = dc_level_view_gpu(view);
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, original[0], &west, err, sizeof(err)));
    ASSERT_EQ(west.chunk.x, -1);

    ASSERT_TRUE(dc_players_enable_at(view, 64u, 40u, false, err, sizeof(err)));
    fr_player_t player;
    fr_beaver_body_t beaver;
    ASSERT_TRUE(dc_players_status(view, 0, &player, &beaver));
    ASSERT_TRUE(fr_gpu_pose_authored_rigid(gpu, beaver.binding.body_id, (dc_chunk_coord_t){-1, 1},
        57 * (int32_t)DC_FLUID_FULL, 4 * (int32_t)DC_FLUID_FULL, 0.f, err, sizeof(err)));
    fr_player_point_t trunk = {{-1, 1}, 52.f, 10.f};
    ASSERT_TRUE(dc_players_aim_override(view, 0, &trunk, err, sizeof(err)));
    SDL_Event key = {.key = {.type = SDL_KEYDOWN, .keysym.scancode = SDL_SCANCODE_C}};
    ASSERT_TRUE(dc_players_event(view, &key));
    for (uint32_t i = 0; i < 4u; ++i) {
        dc_players_frame_time(view, .16);
        ASSERT_TRUE(dc_players_step(view, 1.f/60.f, err, sizeof(err)));
    }
    fr_player_inventory_t inventory;
    ASSERT_TRUE(dc_players_inventory(view, 0, &inventory));
    ASSERT_EQ(fr_player_inventory_mass(&inventory, DC_ITEM_WOOD),
              (uint64_t)llround(DC_DENSITY_WOOD * DC_FLUID_FULL));
    ASSERT_TRUE(read_cell(-12, 74, &cell) && cell.material == DC_MATERIAL_AIR);
    key.type = SDL_KEYUP;
    ASSERT_TRUE(dc_players_event(view, &key));
    ASSERT_TRUE(dc_gpu_remove_body(gpu, original[1], err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)) && dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(close_world());

    view = dc_play_scene_create(directory, 314u, err, sizeof(err));
    ASSERT_TRUE(view && dc_level_view_restored(view));
    ASSERT_TRUE(read_cell(-12, 74, &cell) && cell.material == DC_MATERIAL_AIR);
    ASSERT_TRUE(read_cell(-20, 96, &cell) && cell.material == DC_MATERIAL_WOOD);
    ASSERT_TRUE(read_cell(148, 74, &cell) && cell.material == DC_MATERIAL_WOOD);
    uint32_t restored[3] = {0};
    ASSERT_TRUE(rocks(restored, &owned));
    ASSERT_EQ(owned, 3u);
    ASSERT_EQ(restored[0], original[0]);
    ASSERT_EQ(restored[1], 0u);
    ASSERT_EQ(restored[2], original[2]);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000u, err, sizeof(err)));
    ASSERT_EQ(dc_gpu_find_active_chunk(dc_level_view_gpu(view), (dc_chunk_coord_t){-1, 1}),
              UINT32_MAX);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000u, err, sizeof(err)));
    ASSERT_TRUE(read_cell(-12, 74, &cell) && cell.material == DC_MATERIAL_AIR);
    ASSERT_TRUE(read_cell(-20, 96, &cell) && cell.material == DC_MATERIAL_WOOD);
    PASS();
}

int main(void) {
    printf("=== Authored Play Spawn Tests ===\n");
    RUN(test_play_loads_authored_trees_and_preserves_harvest_and_rocks);
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
