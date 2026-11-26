#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dungeoncraft/material_density.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/player/beaver.h"
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

static bool wait_for_play_start(int output) {
    static const char marker[] = "Propagated lighting:";
    const uint64_t deadline = SDL_GetTicks64() + 300000u;
    struct pollfd stream = {.fd = output, .events = POLLIN};
    size_t matched = 0;
    char text[1024];
    while (SDL_GetTicks64() < deadline) {
        int ready = poll(&stream, 1, 1000);
        if (ready < 0 && errno != EINTR) return false;
        if (ready <= 0) continue;
        ssize_t bytes = read(output, text, sizeof(text));
        if (bytes < 0 && errno == EINTR) continue;
        if (bytes <= 0) return false;
        fwrite(text, 1, (size_t)bytes, stdout);
        for (ssize_t i = 0; i < bytes; ++i) {
            if (text[i] == marker[matched]) {
                if (++matched == sizeof(marker) - 1u) return true;
            } else matched = text[i] == marker[0] ? 1u : 0u;
        }
    }
    return false;
}

static bool launch_play(const char *directory) {
    int output[2];
    if (pipe(output) != 0) return false;
    pid_t child = fork();
    if (child < 0) {
        close(output[0]);
        close(output[1]);
        return false;
    }
    if (!child) {
        close(output[0]);
        if (dup2(output[1], STDOUT_FILENO) < 0) _exit(126);
        close(output[1]);
        execl("./build/dungeoncraft_quarter_native", "dungeoncraft_quarter_native",
              "--play", "--world-dir", directory, (char *)NULL);
        _exit(127);
    }
    close(output[1]);
    bool ready = wait_for_play_start(output[0]);
    bool stopped = kill(child, ready ? SIGTERM : SIGKILL) == 0;
    if (ready && stopped) {
        char text[1024];
        ssize_t bytes;
        do {
            bytes = read(output[0], text, sizeof(text));
            if (bytes > 0) fwrite(text, 1, (size_t)bytes, stdout);
        } while (bytes > 0 || (bytes < 0 && errno == EINTR));
    }
    close(output[0]);
    int status;
    pid_t finished;
    do finished = waitpid(child, &status, 0);
    while (finished < 0 && errno == EINTR);
    bool okay = ready && stopped && finished == child && WIFEXITED(status) &&
                WEXITSTATUS(status) == 0;
    if (!okay) snprintf(err, sizeof(err), "Normal play launch or graceful shutdown failed");
    return okay;
}

static bool open_world(const char *directory) {
    view = dc_play_scene_create(directory, 314u, err, sizeof(err));
    return view && dc_level_view_restored(view) &&
           dc_level_view_pan_pixels(view, -(int32_t)DC_CHUNK_SIDE, 0) &&
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

static bool aim_at_rock(dc_gpu_t *gpu, const dc_gpu_world_body_t *rock) {
    const int64_t left = rock->chunk.x * DC_CHUNK_SIDE + rock->body.x_fp / DC_FLUID_FULL;
    const int64_t top = rock->chunk.y * DC_CHUNK_SIDE + rock->body.y_fp / DC_FLUID_FULL;
    for (uint32_t y = 0; y < rock->body.height; ++y)
        for (uint32_t x = 0; x < rock->body.width; ++x) {
            dc_chunk_coord_t chunk;
            uint32_t lx, ly;
            dc_cell_address(left + x, top + y, &chunk, &lx, &ly);
            uint32_t slot = dc_gpu_find_active_chunk(gpu, chunk);
            if (slot == UINT32_MAX) continue;
            uint32_t tile = gpu->slot_page[slot], occupant;
            if (!dc_gpu_read_occupancy(gpu, tile % gpu->page_width * DC_CHUNK_SIDE + lx,
                tile / gpu->page_width * DC_CHUNK_SIDE + ly, &occupant, NULL, err, sizeof(err)))
                return false;
            if (occupant == rock->body.id) {
                fr_player_point_t aim = {chunk, (float)lx, (float)ly};
                return dc_players_aim_override(view, 0, &aim, err, sizeof(err));
            }
        }
    return false;
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
    ASSERT_TRUE(fr_gpu_pose_authored_rigid(gpu, beaver.binding.body_id, (dc_chunk_coord_t){0, 1},
        DC_FLUID_FULL, 6 * (int32_t)DC_FLUID_FULL, 0.f, err, sizeof(err)));
    ASSERT_TRUE(aim_at_rock(gpu,&west));
    key.type = SDL_KEYDOWN;
    key.key.keysym.scancode = SDL_SCANCODE_G;
    ASSERT_TRUE(dc_players_event(view,&key));
    ASSERT_TRUE(dc_players_step(view,1.f/60.f,err,sizeof(err)));
    key.type = SDL_KEYUP;
    ASSERT_TRUE(dc_players_event(view,&key));
    ASSERT_TRUE(dc_players_step(view,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu,original[0],&west,err,sizeof(err)));
    ASSERT_EQ(west.chunk.x,0);
    ASSERT_EQ(west.chunk.y,0);
    ASSERT_EQ(west.body.x_fp,DC_FLUID_FULL);
    ASSERT_EQ(west.body.y_fp,62 * DC_FLUID_FULL);
    key.type = SDL_KEYDOWN;
    key.key.keysym.scancode = SDL_SCANCODE_E;
    ASSERT_TRUE(dc_players_event(view,&key));
    ASSERT_TRUE(dc_players_step(view,1.f/60.f,err,sizeof(err)));
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,original[0],&motion,err,sizeof(err)));
    ASSERT_TRUE(!(motion.flags & DC_GPU_BODY_KINEMATIC));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu,gpu->body_origin,err,sizeof(err)));
    ASSERT_TRUE(aim_at_rock(gpu,&west));
    key.key.keysym.scancode = SDL_SCANCODE_G;
    ASSERT_TRUE(dc_players_event(view,&key));
    ASSERT_TRUE(dc_players_step(view,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,original[0],&motion,err,sizeof(err)));
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
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
    ASSERT_TRUE(dc_gpu_read_body_motion(dc_level_view_gpu(view),restored[0],&motion,err,sizeof(err)));
    ASSERT_TRUE(!(motion.flags & DC_GPU_BODY_KINEMATIC));
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

static void test_play_combustion_changes_fur_and_reloads_its_gas(void) {
    enum { PAW_X = 26, PAW_Y = 30 };
    const dc_chunk_coord_t air_chunk = {1, 0};
    char directory[] = "build/play_reactive_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    view = dc_play_scene_create(directory, 314u, err, sizeof(err));
    ASSERT_TRUE(view && dc_level_view_tick(view, err, sizeof(err)));
    dc_gpu_t *gpu = dc_level_view_gpu(view);
    fr_player_t player;
    fr_beaver_body_t beaver;
    ASSERT_TRUE(dc_players_status(view, 0, &player, &beaver));
    fr_beaver_source_t fur;
    ASSERT_TRUE(fr_beaver_source(PAW_Y * FR_BEAVER_WIDTH + PAW_X + 1u, &fur));
    ASSERT_EQ(fur.region, FR_BEAVER_FUR);
    const uint32_t x = PAW_X - beaver.rest_origin_x, y = PAW_Y - beaver.rest_origin_y;
    const uint32_t body = beaver.binding.body_id;
    fr_reactive_record_t initial, burned;
    bool found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, body, x, y, &initial, &found, err, sizeof(err)) && found);
    ASSERT_EQ(initial.state.char_mass_fp, 0u);
    ASSERT_EQ(initial.state.flags & FR_REACTIVE_FLAMING, 0u);
    fr_reactive_state_t hot = initial.state;
    const double heat = fr_reactive_state_energy_at_temperature(&hot, 1150.f)
                      - fr_reactive_state_energy(&hot);
    ASSERT_TRUE(fr_reactive_state_add_heat(&hot, heat, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_body(gpu, body, x, y, &hot, err, sizeof(err)));
    for (uint32_t step = 0; step < 24u; ++step)
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)) && dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, body, x, y, &burned, &found, err, sizeof(err)) && found);
    ASSERT_EQ(burned.state.parcel_id_low, initial.state.parcel_id_low);
    ASSERT_EQ(burned.state.parcel_id_high, initial.state.parcel_id_high);
    ASSERT_TRUE(burned.state.fuel_mass_fp < initial.state.fuel_mass_fp);
    ASSERT_TRUE(burned.state.char_mass_fp > 0u);

    const uint32_t slot = dc_gpu_find_active_chunk(gpu, air_chunk);
    fr_gas_checkpoint_chunk_t before, after;
    ASSERT_TRUE(slot != UINT32_MAX && fr_gas_gpu_export_checkpoint(gpu, slot, &before, err, sizeof(err)));
    ASSERT_TRUE(before.clock.physical_steps > 0u && before.clock.advanced_seconds > 0.);
    ASSERT_TRUE(close_world());
    view = dc_play_scene_create(directory, 314u, err, sizeof(err));
    ASSERT_TRUE(view && dc_level_view_restored(view));
    gpu = dc_level_view_gpu(view);
    ASSERT_TRUE(dc_players_status(view, 0, &player, &beaver));
    ASSERT_EQ(beaver.binding.body_id, body);
    fr_reactive_record_t restored;
    ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, body, x, y, &restored, &found, err, sizeof(err)) && found);
    ASSERT_EQ(memcmp(&burned.owner, &restored.owner, sizeof(burned.owner)), 0);
    ASSERT_EQ(memcmp(&burned.state, &restored.state, sizeof(burned.state)), 0);
    const uint32_t restored_slot = dc_gpu_find_active_chunk(gpu, air_chunk);
    ASSERT_TRUE(restored_slot != UINT32_MAX &&
                fr_gas_gpu_export_checkpoint(gpu, restored_slot, &after, err, sizeof(err)));
    ASSERT_EQ(after.clock.physical_steps, before.clock.physical_steps);
    ASSERT_EQ(after.clock.substeps, before.clock.substeps);
    ASSERT_EQ(after.clock.advanced_seconds, before.clock.advanced_seconds);
    ASSERT_EQ(memcmp(before.cells, after.cells, sizeof(before.cells)), 0);
    ASSERT_EQ(memcmp(before.gates, after.gates, sizeof(before.gates)), 0);
    PASS();
}

static void test_play_admits_ambient_air_when_reopening_an_older_save(void) {
    char directory[] = "build/play_legacy_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    const uint32_t surface = 96;
    dc_ground_profile_t ground = {.width = 1, .soil_depth = 1, .surface = &surface};
    view = dc_level_view_create_ground(directory, 314u, &ground, err, sizeof(err));
    ASSERT_TRUE(view &&
                dc_level_view_pan_pixels(view, 0, -INITIAL_CHUNK_Y * (int32_t)DC_CHUNK_SIDE) &&
                dc_level_view_wait_visible(view, 120000u, err, sizeof(err)) &&
                dc_players_enable(view, err, sizeof(err)));
    fr_player_t player;
    fr_beaver_body_t original, restored;
    ASSERT_TRUE(dc_players_status(view, 0, &player, &original));
    ASSERT_TRUE(close_world());

    view = dc_play_scene_create(directory, 314u, err, sizeof(err));
    ASSERT_TRUE(view && dc_level_view_restored(view));
    ASSERT_TRUE(dc_players_status(view, 0, &player, &restored));
    ASSERT_EQ(restored.binding.body_id, original.binding.body_id);
    dc_gpu_t *gpu = dc_level_view_gpu(view);
    const uint32_t slot = dc_gpu_find_active_chunk(gpu, (dc_chunk_coord_t){0, 0});
    fr_gas_checkpoint_chunk_t air;
    ASSERT_TRUE(slot != UINT32_MAX && fr_gas_gpu_export_checkpoint(gpu, slot, &air, err, sizeof(err)));
    ASSERT_TRUE(air.cells[0][0] > 0.f);
    ASSERT_EQ(air.clock.physical_steps, 0u);
    PASS();
}

int main(void) {
    printf("=== Authored Play Spawn Tests ===\n");
    RUN(test_play_loads_authored_trees_and_preserves_harvest_and_rocks);
    RUN(test_play_combustion_changes_fur_and_reloads_its_gas);
    RUN(test_play_admits_ambient_air_when_reopening_an_older_save);
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
