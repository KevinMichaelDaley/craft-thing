#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

#include "../../src/app/level.h"
#include "../../src/app/view_config.h"
#include "ferrum/structural/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %d: %s\n", __LINE__, #e); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_authoritative_chunk_upload_does_not_reseed_fragment_cells(void) {
    char err[256]; dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 128, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk)); ASSERT_TRUE(chunk);
    const unsigned cell = 20 * DC_CHUNK_SIDE + 20;
    chunk->cells[cell].material = DC_MATERIAL_MORTAR_DEBRIS;
    dc_chunk_particle_init(chunk->particles, chunk->coord, cell, DC_MATERIAL_MORTAR_DEBRIS);
    chunk->particles[0].flags |= DC_MPM_FRAGMENT_FLAG;
    chunk->particle_count = 1;
    const unsigned mass = chunk->particles[0].mass_fp;
    bool restored;
    ASSERT_TRUE(fr_gpu_upload_structure_chunk(gpu, 0, chunk, &restored, err, sizeof(err)));
    ASSERT_TRUE(!restored);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    unsigned total = 0;
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) total += chunk->particles[i].mass_fp;
    ASSERT_EQ(chunk->particle_count, 1);
    ASSERT_EQ(total, mass);
    free(chunk); dc_gpu_destroy(gpu); PASS();
}

static void test_native_world_reopens_confirmed_grace_without_reset(void) {
    char directory[] = "build/ui_structure_store_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    fr_structural_gpu_graph_t graph = {.node_count = 2, .member_count = 1};
    graph.nodes[0] = (fr_structural_gpu_node_t){.id = 1, .generation = 1,
        .chunk = {0, INITIAL_CHUNK_Y}, .x_fp = 20 << 16, .y_fp = 40 << 16};
    graph.nodes[1] = graph.nodes[0]; graph.nodes[1].id = 2;
    graph.nodes[1].x_fp += 12 << 16;
    graph.members[0] = (fr_structural_gpu_member_t){1, 1, 1, 2,
        FR_STRUCTURAL_WOOD, 2 << 16, 0, 0};
    ASSERT_TRUE(dc_level_view_place_structure(view, &graph, err, sizeof(err)));
    for (unsigned i = 0; i < 30; i++) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    dc_gpu_world_body_t body; dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_level_view_body_state(view, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                         &body, &motion, err, sizeof(err)));
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    view = dc_level_view_create(directory, 314, err, sizeof(err)); ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    for (unsigned i = 0; i < 59; i++) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_body_state(view, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                         &body, &motion, err, sizeof(err)));
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_body_state(view, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                         &body, &motion, err, sizeof(err)));
    ASSERT_TRUE(!(motion.flags & DC_GPU_BODY_KINEMATIC));
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

int main(void) {
    RUN(test_authoritative_chunk_upload_does_not_reseed_fragment_cells);
    RUN(test_native_world_reopens_confirmed_grace_without_reset);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
