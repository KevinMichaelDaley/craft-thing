#define main placement_fixture_main
#include "placement_tests.c"
#undef main
#include <SDL.h>
#include "../vulkan/compound_fixture.h"

static fr_structural_gpu_node_t scene_node(uint32_t id, int x, int y) {
    return (fr_structural_gpu_node_t){.id = id,
                                      .generation = 1,
                                      .chunk = {x / 64, y / 64},
                                      .x_fp = (x % 64) << 16,
                                      .y_fp = (y % 64) << 16};
}
static fr_structural_gpu_graph_t supported_frame(void) {
    fr_structural_gpu_graph_t graph = {.node_count = 5,
                                       .member_count = 3,
                                       .joint_count = 1,
                                       .anchor_count = 2};
    graph.nodes[0] = scene_node(1, 120, 80);
    graph.nodes[1] = scene_node(2, 136, 80);
    graph.nodes[2] = scene_node(3, 152, 80);
    graph.nodes[3] = scene_node(4, 160, 94);
    graph.nodes[4] = scene_node(5, 154, 80);
    graph.members[0] = (fr_structural_gpu_member_t){
        1, 1, 1, 2, FR_STRUCTURAL_WOOD, 5 << 16, 0, 0};
    graph.members[1] = (fr_structural_gpu_member_t){
        2, 1, 2, 3, FR_STRUCTURAL_WOOD, 5 << 16, 0, 0};
    graph.members[2] = (fr_structural_gpu_member_t){
        3, 1, 4, 5, FR_STRUCTURAL_WOOD, 4 << 16, 0, 0};
    graph.anchors[0] = beam(20).anchors[0];
    graph.anchors[0].chunk = (dc_chunk_coord_t){1, 1};
    graph.anchors[0].x = 55;
    graph.anchors[0].y = 16;
    graph.anchors[1] = graph.anchors[0];
    graph.anchors[1].id = 2;
    graph.anchors[1].node = 4;
    graph.anchors[1].chunk.x = 2;
    graph.anchors[1].x = 33;
    graph.anchors[1].y = 30;
    graph.joints[0] = (fr_structural_gpu_joint_t){.id = 1,
                                                  .generation = 1,
                                                  .a = 3,
                                                  .b = 5,
                                                  .kind = FR_JOINT_MOMENT,
                                                  .normal_stiffness = 1e8,
                                                  .shear_stiffness = 1e8,
                                                  .rotational_stiffness = 1e8,
                                                  .bearing_capacity = 1e7,
                                                  .pullout_capacity = 1e7,
                                                  .shear_capacity = 1e7,
                                                  .moment_capacity = 1e7,
                                                  .friction = .5};
    return graph;
}
static bool scene_terrain(dc_gpu_t *gpu, char *err) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk)
        return false;
    bool okay = true;
    for (unsigned slot = 0; okay && slot < 70; slot++) {
        memset(chunk, 0, sizeof(*chunk));
        chunk->coord = (dc_chunk_coord_t){slot % 10, slot / 10};
        for (unsigned y = 0; y < 64; y++)
            for (unsigned x = 0; x < 64; x++) {
                unsigned wx = slot % 10 * 64 + x, wy = slot / 10 * 64 + y;
                if (wy >= 100 || (wx == 119 && wy >= 80) ||
                    (wx == 161 && wy >= 94))
                    chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
            }
        okay = dc_gpu_upload_chunk(gpu, slot, chunk, err, 256) &&
               dc_gpu_set_page(gpu, slot % 10, slot / 10, slot, err, 256);
    }
    free(chunk);
    return okay;
}
static bool scene_tick(dc_gpu_t *gpu, char *err) {
    uint64_t start = SDL_GetPerformanceCounter(),
             frequency = SDL_GetPerformanceFrequency();
    bool okay = dc_gpu_present_chunks_steps(gpu, 1, err, 256);
    uint64_t elapsed = SDL_GetPerformanceCounter() - start,
             interval = frequency / 60;
    if (okay && elapsed < interval)
        SDL_Delay((uint32_t)((interval - elapsed) * 1000 / frequency));
    return okay;
}
static bool scene_snapshot(dc_gpu_t *gpu, const char *path, char *err) {
    uint32_t *pixels = malloc(160 * 112 * sizeof(*pixels));
    bool okay = pixels && dc_gpu_readback(gpu, pixels, 160 * 112, err, 256);
    if (okay) {
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
            pixels, 160, 112, 32, 160 * 4, SDL_PIXELFORMAT_ABGR8888);
        okay = surface && SDL_SaveBMP(surface, path) == 0;
        if (surface)
            SDL_FreeSurface(surface);
    }
    free(pixels);
    return okay;
}
static void
test_two_concave_bodies_on_sideways_beam_and_angled_brace_collapse_visibly(
    void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid();
    ASSERT_TRUE(gpu);
    bool overlay = dc_gpu_set_overlay(gpu, (dc_gpu_overlay_t)4);
    if (!overlay)
        dc_gpu_destroy(gpu);
    ASSERT_TRUE(overlay);
    ASSERT_TRUE(dc_gpu_set_viewport(gpu, 80, 0, 160, 112));
    ASSERT_TRUE(scene_terrain(gpu, err));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(gpu, true));
    fr_structural_gpu_graph_t graph = supported_frame();
    bool placed = fr_gpu_place_structure(gpu, &graph, err, sizeof(err));
    if (!placed)
        printf("scene placement: %s\n", err);
    ASSERT_TRUE(placed);
    dc_gpu_compound_shape_t l = {.count = 2}, u = {.count = 3};
    l.pieces[0] = fixture_rectangle(DC_GPU_BODY_STONE, 0, 0, 3, 16);
    l.pieces[1] = fixture_rectangle(DC_GPU_BODY_STONE, 3, 13, 13, 3);
    u.pieces[0] = fixture_rectangle(DC_GPU_BODY_STONE, 0, 0, 3, 16);
    u.pieces[1] = fixture_rectangle(DC_GPU_BODY_STONE, 13, 0, 3, 16);
    u.pieces[2] = fixture_rectangle(DC_GPU_BODY_STONE, 3, 13, 10, 3);
    dc_gpu_world_body_t left = fixture_body(1, 120, 61),
                        right = fixture_body(2, 136, 61);
    left.body.y_fp += 32768;
    right.body.y_fp += 32768;
    ASSERT_TRUE(dc_gpu_spawn_compound_body(gpu, left, &l, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_compound_body(gpu, right, &u, err, sizeof(err)));
    for (unsigned tick = 0; tick < 60; tick++)
        ASSERT_TRUE(scene_tick(gpu, err));
    dc_gpu_world_body_t held_l, held_u;
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 1, &held_l, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 2, &held_u, err, sizeof(err)));
    ASSERT_TRUE(abs(held_l.body.y_fp - left.body.y_fp) < 65536);
    ASSERT_TRUE(abs(held_u.body.y_fp - right.body.y_fp) < 65536);
    fr_structural_gpu_member_state_t supported;
    ASSERT_TRUE(fr_gpu_read_structure_member(gpu, 1, &supported));
    printf("braced utilization %.6f damage %.6f\n", supported.utilization,
           supported.damage);
    ASSERT_TRUE(supported.utilization < 1 && supported.damage == 0);
    uint32_t pixels[160 * 112];
    ASSERT_TRUE(dc_gpu_readback(gpu, pixels, 160 * 112, err, sizeof(err)));
    ASSERT_TRUE((pixels[65 * 160 + 41] & 255u) > 120u);
    ASSERT_TRUE((pixels[65 * 160 + 45] & 255u) < 40u);
    ASSERT_TRUE((pixels[65 * 160 + 57] & 255u) > 120u);
    ASSERT_TRUE((pixels[65 * 160 + 61] & 255u) < 40u);
    ASSERT_TRUE(scene_snapshot(gpu, "build/structure_braced.bmp", err));
    graph.node_count = 3;
    graph.member_count = 2;
    graph.anchor_count = 1;
    graph.joint_count = 0;
    ASSERT_TRUE(fr_gpu_place_structure(gpu, &graph, err, sizeof(err)));
    ASSERT_TRUE(scene_tick(gpu, err));
    fr_structural_gpu_member_state_t overloaded;
    ASSERT_TRUE(fr_gpu_read_structure_member(gpu, 1, &overloaded));
    printf("unbraced utilization %.6f damage %.6f\n", overloaded.utilization,
           overloaded.damage);
    ASSERT_TRUE(overloaded.utilization > 1 && overloaded.damage < 1);
    ASSERT_TRUE(dc_gpu_readback(gpu, pixels, 160 * 112, err, sizeof(err)));
    ASSERT_TRUE((pixels[80 * 160 + 44] & 255u) > 200u);
    ASSERT_TRUE(((pixels[80 * 160 + 44] >> 8) & 255u) < 80u);
    ASSERT_TRUE(scene_snapshot(gpu, "build/structure_unbraced.bmp", err));
    for (unsigned tick = 0; tick < 600; tick++)
        ASSERT_TRUE(scene_tick(gpu, err));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 1, &left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 2, &right, err, sizeof(err)));
    printf("held/final body y: L %.6f -> %.6f x %.6f; U %.6f -> %.6f x %.6f\n",
           64. * held_l.chunk.y + held_l.body.y_fp / 65536.,
           64. * left.chunk.y + left.body.y_fp / 65536.,
           64. * left.chunk.x + left.body.x_fp / 65536.,
           64. * held_u.chunk.y + held_u.body.y_fp / 65536.,
           64. * right.chunk.y + right.body.y_fp / 65536.,
           64. * right.chunk.x + right.body.x_fp / 65536.);
    for (unsigned id = 1; id <= 2; id++) {
        fr_structural_gpu_member_state_t state;
        if (fr_gpu_read_structure_member(gpu, id, &state))
            printf("member %u state %u util %.6f damage %.6f\n", id,
                   state.state, state.utilization, state.damage);
    }
    ASSERT_TRUE(scene_snapshot(gpu, "build/structure_collapsed.bmp", err));
    ASSERT_TRUE(64. * left.chunk.y + left.body.y_fp / 65536. >
                64. * held_l.chunk.y + held_l.body.y_fp / 65536. + 6.);
    ASSERT_TRUE(64. * right.chunk.y + right.body.y_fp / 65536. >
                64. * held_u.chunk.y + held_u.body.y_fp / 65536. + 6.);
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, 1, &motion, err, sizeof(err)));
    ASSERT_TRUE(fabsf(motion.mass - 234.9f) < .001f);
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, 2, &motion, err, sizeof(err)));
    ASSERT_TRUE(fabsf(motion.mass - 340.2f) < .001f);
    dc_gpu_contact_t *contacts=calloc(DC_GPU_CONTACT_CAPACITY,sizeof(*contacts));
    ASSERT_TRUE(contacts);
    dc_gpu_contact_stats_t stats;
    ASSERT_TRUE(dc_gpu_read_contacts(gpu,&stats,contacts,DC_GPU_CONTACT_CAPACITY,err,sizeof(err)));
    float support_low=INFINITY,support_high=-INFINITY;
    uint32_t support_count=0;
    for (uint32_t i=0;i<stats.count;i++) {
        dc_gpu_contact_t c=contacts[i];
        dc_gpu_contact_anchor_t p;
        if (c.body_a==2 && c.normal_y<-.5f) p=c.anchor_a;
        else if (c.body_b==2 && c.normal_y>.5f) p=c.anchor_b;
        else continue;
        float x=(float)(64.*p.chunk.x+p.x_fp/65536.);
        support_low=fminf(support_low,x);support_high=fmaxf(support_high,x);support_count++;
    }
    free(contacts);
    float com_x=(float)(64.*right.chunk.x+right.body.x_fp/65536.+motion.center_x);
    float com_y=(float)(64.*right.chunk.y+right.body.y_fp/65536.+motion.center_y);
    float lowest=-INFINITY;
    for (uint32_t p=0;p<u.count;p++) for (uint32_t v=0;v<u.pieces[p].count;v++) {
        float x=u.pieces[p].vertices[v].x_fp/65536.f-motion.center_x;
        float y=u.pieces[p].vertices[v].y_fp/65536.f-motion.center_y;
        float wx=com_x+cosf(motion.angle)*x-sinf(motion.angle)*y;
        float wy=com_y+sinf(motion.angle)*x+cosf(motion.angle)*y;
        lowest=fmaxf(lowest,wy);
        /* The resting solver permits a subpixel contact skin. Derive support
         * from actual shape vertices against this scene's known flat floor,
         * independently of whether the final manifold includes speculative contacts. */
        if (fabsf(wy-100.f)<=.05f) {
            support_low=fminf(support_low,wx);support_high=fmaxf(support_high,wx);support_count++;
        }
    }
    bool moving=right.body.vx_fp!=0 || right.body.vy_fp!=0 || fabsf(motion.angular_velocity)>1e-6f;
    printf("right U rest support: COMx %.6f support[%.6f,%.6f] witnesses%u angle%.6f omega%.9f v(%d,%d)\n",
        com_x,support_low,support_high,support_count,motion.angle,motion.angular_velocity,right.body.vx_fp,right.body.vy_fp);
    ASSERT_TRUE(moving || (support_count>0 && com_x>=support_low-.01f && com_x<=support_high+.01f));
    ASSERT_TRUE(!moving);
    ASSERT_TRUE(lowest<=100.05f && lowest>=99.95f);
    ASSERT_TRUE(scene_snapshot(gpu, "build/structure_collapsed.bmp", err));
    dc_gpu_destroy(gpu);
    PASS();
}
int main(void) {
    RUN(test_two_concave_bodies_on_sideways_beam_and_angled_brace_collapse_visibly);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
