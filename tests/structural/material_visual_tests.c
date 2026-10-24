#define main placement_fixture_main
#include "placement_tests.c"
#undef main
#include <SDL.h>
#include "../../src/vulkan/gpu_internal.h"
#include "../../src/vulkan/runtime_body_gpu.h"

static bool tick(dc_gpu_t *gpu, char *err) {
    uint64_t start = SDL_GetPerformanceCounter(),
             hz = SDL_GetPerformanceFrequency();
    bool okay = dc_gpu_present_chunks_steps(gpu, 1, err, 256);
    uint64_t spent = SDL_GetPerformanceCounter() - start;
    if (okay && spent < hz / 60)
        SDL_Delay((uint32_t)((hz / 60 - spent) * 1000 / hz));
    return okay;
}
static bool snapshot(dc_gpu_t *gpu, const char *path, char *err) {
    uint32_t pixels[160 * 112];
    if (!dc_gpu_readback(gpu, pixels, 160 * 112, err, 256))
        return false;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, 160, 112, 32, 160 * 4, SDL_PIXELFORMAT_ABGR8888);
    SDL_Surface *scaled = SDL_CreateRGBSurfaceWithFormat(
        0, 640, 448, 32, SDL_PIXELFORMAT_ABGR8888);
    bool okay = surface && scaled &&
                SDL_BlitScaled(surface, NULL, scaled, NULL) == 0 &&
                SDL_SaveBMP(scaled, path) == 0;
    if (scaled)
        SDL_FreeSurface(scaled);
    if (surface)
        SDL_FreeSurface(surface);
    return okay;
}
static void
test_material_profiles_produce_visible_splitting_cracking_and_crumble(void) {
    const uint32_t materials[] = {FR_STRUCTURAL_WOOD, FR_STRUCTURAL_STONE,
                                  FR_STRUCTURAL_MORTAR};
    const char *paths[] = {"build/structure_wood_split.bmp",
                           "build/structure_stone_crack.bmp",
                           "build/structure_mortar_crumble.bmp"};
    char err[256] = {0};
    for (unsigned material = 0; material < 3; ++material) {
        dc_gpu_t *gpu = grid();
        ASSERT_TRUE(gpu);
        ASSERT_TRUE(dc_gpu_set_overlay(gpu, DC_GPU_OVERLAY_STRUCTURAL_STRESS));
        ASSERT_TRUE(dc_gpu_set_rigid_solver(gpu, true));
        dc_chunk_t *terrain = calloc(1, sizeof(*terrain));
        ASSERT_TRUE(terrain);
        for (unsigned x = 0; x < 64; x++)
            terrain->cells[62 * 64 + x].material = DC_MATERIAL_STONE;
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, terrain, err, 256));
        free(terrain);
        fr_structural_gpu_graph_t graph = beam(20);
        graph.members[0].material = materials[material];
        ASSERT_TRUE(
            dc_gpu_paint_material(gpu, 20, 41, 0, DC_MATERIAL_STONE, err, 256));
        ASSERT_TRUE(fr_gpu_place_structure(gpu, &graph, err, 256));
        if (material == 0) {
            dc_gpu_body_t load = {.x_fp = 24 << 16,
                                  .y_fp = 23 << 16,
                                  .width = 14,
                                  .height = 16,
                                  .id = 1,
                                  .active = 1};
            ASSERT_TRUE(dc_gpu_spawn_body(gpu, load, err, 256));
        }
        fr_structural_gpu_member_state_t state;
        for (unsigned frame = 0; frame < 20; ++frame) {
            ASSERT_TRUE(tick(gpu, err));
            ASSERT_TRUE(fr_gpu_read_structure_member(gpu, 1, &state));
            if (state.state == FR_STRUCTURAL_CONVERTED)
                break;
        }
        ASSERT_EQ(state.state, FR_STRUCTURAL_CONVERTED);
        if (material < 2) {
            dc_gpu_body_motion_t a, b;
            ASSERT_TRUE(dc_gpu_read_body_motion(
                gpu, FR_STRUCTURAL_BODY_ID_BASE + 1, &a, err, 256));
            ASSERT_TRUE(dc_gpu_read_body_motion(
                gpu, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, &b, err, 256));
            ASSERT_TRUE(fabs(a.mass + b.mass - (material ? 64.8 : 14.4)) <
                        .002);
        } else {
            dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
            ASSERT_TRUE(chunk &&
                        dc_gpu_download_chunk(gpu, 0, chunk, err, 256));
            ASSERT_EQ(chunk->particle_count, 24);
            free(chunk);
        }
        for (unsigned frame = 0; frame < 48; frame++)
            ASSERT_TRUE(tick(gpu, err));
        ASSERT_TRUE(snapshot(gpu, paths[material], err));
        dc_gpu_destroy(gpu);
    }
    PASS();
}
static void
test_wood_floats_and_stone_sinks_visibly_from_material_density(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid();
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(gpu && chunk);
    for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 64; x++) {
            dc_cell_t *cell = &chunk->cells[y * 64 + x];
            if (x == 0 || x == 63 || y == 63)
                cell->material = DC_MATERIAL_STONE;
            else if (y >= 20)
                cell->fluid_mass = DC_FLUID_FULL;
        }
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, 256));
    ASSERT_TRUE(dc_gpu_set_overlay(gpu, DC_GPU_OVERLAY_STRUCTURAL_STRESS));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(gpu, true));
    for (unsigned i = 0; i < 2; i++) {
        dc_gpu_body_shape_t shape = {
            .count = 4,
            .material = i ? DC_GPU_BODY_STONE : DC_GPU_BODY_WOOD,
            .vertices = {
                {0, 0}, {4 << 16, 0}, {4 << 16, 4 << 16}, {0, 4 << 16}}};
        dc_gpu_world_body_t body = {
            .body = {.x_fp = (int32_t)((20 + i * 20) << 16),
                     .y_fp = 18 << 16,
                     .width = 4,
                     .height = 4,
                     .id = 100 + i,
                     .active = 1}};
        ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, body, &shape, err, 256));
    }
    ASSERT_TRUE(
        snapshot(gpu, "build/structure_material_water_before.bmp", err));
    for (unsigned frame = 0; frame < 600; frame++)
        ASSERT_TRUE(tick(gpu, err));
    dc_gpu_body_t wood, stone;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 100, &wood, err, 256));
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 101, &stone, err, 256));
    printf("Visual wet draft: wood y %.4f stone y %.4f\n", wood.y_fp / 65536.0,
           stone.y_fp / 65536.0);
    ASSERT_TRUE(wood.y_fp > 17 << 16 && wood.y_fp < 20 << 16);
    ASSERT_TRUE(stone.y_fp > 55 << 16);
    ASSERT_TRUE(snapshot(gpu, "build/structure_material_water_after.bmp", err));
    uint32_t reference[160 * 112], stable[160 * 112];
    ASSERT_TRUE(dc_gpu_readback(gpu, reference, 160 * 112, err, 256));
    for (unsigned frame = 0; frame < 8; frame++) {
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 0, err, 256));
        ASSERT_TRUE(dc_gpu_readback(gpu, stable, 160 * 112, err, 256));
        ASSERT_EQ(memcmp(reference, stable, sizeof(reference)), 0);
    }
    uint64_t water = 0;
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, 256));
    for (unsigned i = 0; i < 4096; i++)
        water += chunk->cells[i].fluid_mass;
    ASSERT_EQ(water, 43u * 62u * (uint64_t)DC_FLUID_FULL);
    dc_gpu_memory_stats_t memory;
    dc_gpu_tick_capture_t capture;
    ASSERT_TRUE(dc_gpu_memory_stats(gpu, &memory));
    printf("Wet visual memory: mapped %.3f MiB, device %.3f MiB\n",
           memory.mapped_local_bytes/1048576.,memory.device_only_bytes/1048576.);
    /* Preserve the pre-channel physics allowance and account for required
       authored-color planes and addressed body/terrain bookkeeping separately. */
    VkDeviceSize appearance=2u*dc_gpu_color_terrain_bytes()+2u*dc_gpu_color_bank_bytes(gpu);
    VkDeviceSize dynamic=(VkDeviceSize)DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS*sizeof(uint32_t)+
        dc_gpu_contact_work_bytes(gpu)+(VkDeviceSize)dc_gpu_body_capacity(gpu)*(256u+32u+512u)+256u*1024u;
    ASSERT_TRUE(memory.mapped_local_bytes+memory.mapped_system_bytes < 72u*1048576u+appearance+dynamic);
    ASSERT_TRUE(memory.device_only_bytes < 112u * 1048576u);
    ASSERT_TRUE(dc_gpu_tick_capture(gpu, &capture, err, 256));
    ASSERT_TRUE(capture.stages[0].gpu_ns > 0);
    printf("Wet visual GPU rigid %.3f ms, mapped %.3f MiB, device %.3f MiB\n",
           capture.stages[0].gpu_ns / 1000000.0,
           memory.mapped_local_bytes / 1048576.0,
           memory.device_only_bytes / 1048576.0);
    free(chunk);
    dc_gpu_destroy(gpu);
    PASS();
}
static void
test_symmetric_and_eccentric_seam_bridge_survives_visual_checkpoint(void) {
    char err[256] = {0};
    for (unsigned eccentric = 0; eccentric < 2; eccentric++) {
        dc_gpu_t *gpu = grid();
        ASSERT_TRUE(gpu);
        ASSERT_TRUE(dc_gpu_set_rigid_solver(gpu, true));
        ASSERT_TRUE(dc_gpu_set_overlay(gpu, DC_GPU_OVERLAY_STRUCTURAL_STRESS));
        fr_structural_gpu_graph_t graph = beam(58);
        graph.anchor_count = 2;
        graph.anchors[0].kind = FR_JOINT_PINNED;
        graph.anchors[1] = graph.anchors[0];
        graph.anchors[1].id = 2;
        graph.anchors[1].node = 2;
        graph.anchors[1].chunk.x = 1;
        graph.anchors[1].x = 6;
        ASSERT_TRUE(
            dc_gpu_paint_material(gpu, 58, 41, 0, DC_MATERIAL_STONE, err, 256));
        ASSERT_TRUE(
            dc_gpu_paint_material(gpu, 70, 41, 0, DC_MATERIAL_STONE, err, 256));
        ASSERT_TRUE(fr_gpu_place_structure(gpu, &graph, err, 256));
        dc_gpu_body_t load = {.id = 17,
                              .x_fp = (int32_t)((eccentric ? 60 : 63) << 16),
                              .y_fp = 37 << 16,
                              .width = 2,
                              .height = 2,
                              .active = 1};
        ASSERT_TRUE(dc_gpu_spawn_body(gpu, load, err, 256));
        ASSERT_TRUE(dc_gpu_set_body_motion(
            gpu, 17, 0, 0, DC_GPU_BODY_LOCK_ROTATION, err, 256));
        double reaction_a = 0, reaction_b = 0;
        for (unsigned frame = 0; frame < 100; frame++) {
            ASSERT_TRUE(tick(gpu, err));
            fr_structural_gpu_node_state_t a, b;
            ASSERT_TRUE(fr_gpu_read_structure_node(gpu, 1, &a));
            ASSERT_TRUE(fr_gpu_read_structure_node(gpu, 2, &b));
            fr_structural_gpu_component_state_t budget;
            ASSERT_TRUE(fr_gpu_read_structure_component(gpu, 1, &budget));
            ASSERT_TRUE(budget.relative_residual < 1e-4);
            ASSERT_TRUE(fabs(budget.external[1] + budget.reaction[1]) < .1);
            if (frame >= 40) {
                reaction_a += a.reaction[1];
                reaction_b += b.reaction[1];
            }
        }
        reaction_a /= 60;
        reaction_b /= 60;
        printf("visual bridge mean reaction %.6f %.6f eccentric %u\n",
               reaction_a, reaction_b, eccentric);
        ASSERT_TRUE(fabs(reaction_a - (eccentric ? -13770 : -11340)) <
                    (eccentric ? 13770.0 : 11340.0) * .02);
        ASSERT_TRUE(fabs(reaction_b - (eccentric ? -8910 : -11340)) <
                    (eccentric ? 8910.0 : 11340.0) * .02);
        ASSERT_TRUE(snapshot(gpu,
                             eccentric ? "build/structure_bridge_eccentric.bmp"
                                       : "build/structure_bridge_symmetric.bmp",
                             err));
        ASSERT_TRUE(
            fr_gpu_save_structure(gpu, "build/structure_seam.dcs", err, 256));
        ASSERT_TRUE(tick(gpu, err));
        uint32_t before[160 * 112], after[160 * 112];
        ASSERT_TRUE(dc_gpu_readback(gpu, before, 160 * 112, err, 256));
        dc_gpu_destroy(gpu);
        gpu = grid();
        ASSERT_TRUE(gpu);
        ASSERT_TRUE(
            fr_gpu_load_structure(gpu, "build/structure_seam.dcs", err, 256));
        ASSERT_TRUE(dc_gpu_set_overlay(gpu, DC_GPU_OVERLAY_STRUCTURAL_STRESS));
        ASSERT_TRUE(
            dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, 256));
        ASSERT_TRUE(tick(gpu, err));
        ASSERT_TRUE(dc_gpu_readback(gpu, after, 160 * 112, err, 256));
        unsigned changed = 0;
        for (unsigned pixel = 0; pixel < 160 * 112; pixel++)
            if (before[pixel] != after[pixel]) {
                if (changed < 8)
                    printf("reload pixel %u,%u %08x/%08x\n", pixel % 160,
                           pixel / 160, before[pixel], after[pixel]);
                changed++;
            }
        printf("reload changed pixels %u\n", changed);
        ASSERT_TRUE(snapshot(gpu, "build/structure_bridge_reloaded.bmp", err));
        ASSERT_EQ(memcmp(before, after, sizeof(before)), 0);
        ASSERT_TRUE(snapshot(gpu, "build/structure_bridge_reloaded.bmp", err));
        dc_gpu_destroy(gpu);
    }
    PASS();
}
int main(void) {
    RUN(test_symmetric_and_eccentric_seam_bridge_survives_visual_checkpoint);
    if (getenv("DC_VISUAL_BRIDGE_ONLY") == NULL) {
        RUN(test_material_profiles_produce_visible_splitting_cracking_and_crumble);
        RUN(test_wood_floats_and_stone_sinks_visibly_from_material_density);
    }
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
