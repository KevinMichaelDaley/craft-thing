#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/gpu.h"
#include "../../src/vulkan/gpu_internal.h"
#include "ferrum/player/manipulation.h"

static int g_pass, g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while (0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define PASS() g_pass++

/* Player anchor on the ground; reticle and carry point derived from it. */
#define PLAYER_X 200.f
#define PLAYER_Y 200.f
#define RETICLE_DX 12.f
#define RETICLE_DY -6.f

static dc_gpu_t *gpu = NULL;
static char err[256];

static bool setup_world(void) {
    err[0] = 0;
    if (!dc_gpu_create(&gpu, 480, 270,
                       "build/shaders/pattern.comp.spv", err, sizeof(err)))
        return false;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return false;
    chunk->coord = (dc_chunk_coord_t){3, 3};
    for (uint32_t x = 0; x < 64u; x++)
        chunk->cells[40u * 64u + x].material = DC_MATERIAL_STONE;
    bool okay = dc_gpu_upload_chunk(gpu, 11, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(gpu, 3, 3, 11, err, sizeof(err));
    free(chunk);
    return okay;
}

static uint32_t spawn_wood(float x, float y, uint32_t width, uint32_t height) {
    dc_gpu_body_t body = {.x_fp = (int32_t)(x * 65536.f),
        .y_fp = (int32_t)(y * 65536.f), .width = width, .height = height,
        .id = 0x80000001u, .active = 1u};
    if (!dc_gpu_spawn_body(gpu, body, err, sizeof(err))) return 0u;
    return body.id;
}

/* Reticle for a world point; the fixture world lives in chunk (3,3). */
static fr_player_point_t reticle_at(float world_x, float world_y) {
    return (fr_player_point_t){.chunk = {3, 3},
        .x = world_x - 192.f, .y = world_y - 192.f};
}
static uint32_t spawn_and_settle(float x, float y, uint32_t width,
                                 uint32_t height) {
    uint32_t id = spawn_wood(x, y, width, height);
    if (!id) return 0u;
    if (!dc_gpu_rigid_step(gpu, err, sizeof(err))) return 0u;
    return id;
}
static fr_player_manipulation_t defaults(void) {
    fr_player_manipulation_t m = {.carry_weight = 60.0, .throw_force = 6.0,
        .reach = 24.0};
    return m;
}
static fr_player_manipulation_t manipulated(double carry, double force) {
    fr_player_manipulation_t m = {.carry_weight = carry,
        .throw_force = force, .reach = 24.0};
    return m;
}

static void test_grab_selects_body_at_reticle_within_reach(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    uint32_t id = spawn_and_settle(PLAYER_X + RETICLE_DX, PLAYER_Y + RETICLE_DY, 4u,
                             4u);
    ASSERT_TRUE(id != 0u);
    fr_player_manipulation_t m = defaults();
    fr_player_point_t reticle = reticle_at(PLAYER_X + RETICLE_DX,
                                           PLAYER_Y + RETICLE_DY);
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &m, PLAYER_X, PLAYER_Y,
                                            &reticle, err, sizeof(err)));
    ASSERT_TRUE(fr_player_manipulation_carrying(&carry, id));
    fr_player_manipulation_reset(&carry);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_grab_refuses_overweight_bodies(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    uint32_t id = spawn_and_settle(PLAYER_X + RETICLE_DX, PLAYER_Y + RETICLE_DY, 12u,
                             12u);
    ASSERT_TRUE(id != 0u);
    fr_player_manipulation_t m = defaults();
    fr_player_point_t reticle = reticle_at(PLAYER_X + RETICLE_DX,
                                           PLAYER_Y + RETICLE_DY);
    ASSERT_TRUE(!fr_player_manipulation_grab(gpu, &carry, &m, PLAYER_X, PLAYER_Y,
                                             &reticle, err, sizeof(err)));
    ASSERT_TRUE(!fr_player_manipulation_carrying(&carry, id));
    fr_player_manipulation_reset(&carry);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_grab_refuses_bodies_beyond_reach(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    uint32_t id = spawn_and_settle(PLAYER_X + 40.f, PLAYER_Y + RETICLE_DY, 4u, 4u);
    ASSERT_TRUE(id != 0u);
    fr_player_manipulation_t m = defaults();
    fr_player_point_t reticle = reticle_at(PLAYER_X + 40.f,
                                           PLAYER_Y + RETICLE_DY);
    ASSERT_TRUE(!fr_player_manipulation_grab(gpu, &carry, &m, PLAYER_X, PLAYER_Y,
                                             &reticle, err, sizeof(err)));
    fr_player_manipulation_reset(&carry);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_carried_body_tracks_the_carry_point(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    uint32_t id = spawn_and_settle(PLAYER_X + RETICLE_DX, PLAYER_Y + RETICLE_DY, 4u,
                             4u);
    ASSERT_TRUE(id != 0u);
    fr_player_manipulation_t m = defaults();
    fr_player_point_t reticle = reticle_at(PLAYER_X + RETICLE_DX,
                                           PLAYER_Y + RETICLE_DY);
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &m, PLAYER_X, PLAYER_Y,
                                            &reticle, err, sizeof(err)));
    /* Two ticks of carry: the body rides the carry point over the player. */
    for (uint32_t tick = 0u; tick < 2u; tick++)
        ASSERT_TRUE(fr_player_manipulation_tick(gpu, &carry, PLAYER_X, PLAYER_Y - 8.f,
                                                err, sizeof(err)));
    dc_gpu_world_body_t body;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, id, &body, err, sizeof(err)));
    float bx = (float)body.chunk.x * 64.f + body.body.x_fp / 65536.f;
    float by = (float)body.chunk.y * 64.f + body.body.y_fp / 65536.f;
    ASSERT_TRUE(fabsf(bx - PLAYER_X) < 12.f && fabsf(by - (PLAYER_Y - 8.f)) < 12.f);
    fr_player_manipulation_reset(&carry);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_drop_releases_the_carry(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    uint32_t id = spawn_and_settle(PLAYER_X + RETICLE_DX, PLAYER_Y + RETICLE_DY, 4u,
                             4u);
    ASSERT_TRUE(id != 0u);
    fr_player_manipulation_t m = defaults();
    fr_player_point_t reticle = reticle_at(PLAYER_X + RETICLE_DX,
                                           PLAYER_Y + RETICLE_DY);
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &m, PLAYER_X, PLAYER_Y,
                                            &reticle, err, sizeof(err)));
    ASSERT_TRUE(fr_player_manipulation_drop(gpu, &carry, err, sizeof(err)));
    ASSERT_TRUE(!fr_player_manipulation_carrying(&carry, id));
    fr_player_manipulation_reset(&carry);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_throw_launches_toward_the_reticle_with_force(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    uint32_t id = spawn_and_settle(PLAYER_X + RETICLE_DX, PLAYER_Y + RETICLE_DY, 4u,
                             4u);
    ASSERT_TRUE(id != 0u);
    fr_player_manipulation_t m = manipulated(60.0, 3.0);
    fr_player_point_t on_body = reticle_at(PLAYER_X + RETICLE_DX,
                                           PLAYER_Y + RETICLE_DY);
    fr_player_point_t aim = reticle_at(PLAYER_X + RETICLE_DX + 20.f,
                                       PLAYER_Y + RETICLE_DY);
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &m, PLAYER_X, PLAYER_Y,
                                            &on_body, err, sizeof(err)));
    ASSERT_TRUE(fr_player_manipulation_throw(gpu, &carry, &m, PLAYER_X, PLAYER_Y,
                                             &aim, err, sizeof(err)));
    ASSERT_TRUE(!fr_player_manipulation_carrying(&carry, id));
    dc_gpu_world_body_t world;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, id, &world, err, sizeof(err)));
    float vx = (float)world.body.vx_fp / 65536.f;
    float vy = (float)world.body.vy_fp / 65536.f;
    float speed = sqrtf(vx * vx + vy * vy);
    /* The throw force is the launch speed toward the reticle. */
    ASSERT_TRUE(speed > 2.5f && speed <= 3.01f);
    ASSERT_TRUE(vx > 0.f);
    fr_player_manipulation_reset(&carry);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_throw_force_attribute_scales_the_launch(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    ASSERT_TRUE(spawn_and_settle(PLAYER_X + RETICLE_DX, PLAYER_Y + RETICLE_DY,
                                 4u, 4u) != 0u);
    fr_player_manipulation_t strong = manipulated(60.0, 4.0);
    fr_player_point_t on_body = reticle_at(PLAYER_X + RETICLE_DX,
                                           PLAYER_Y + RETICLE_DY);
    fr_player_point_t aim = reticle_at(PLAYER_X + RETICLE_DX + 20.f,
                                       PLAYER_Y + RETICLE_DY);
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &strong, PLAYER_X, PLAYER_Y,
                                            &on_body, err, sizeof(err)));
    ASSERT_TRUE(fr_player_manipulation_throw(gpu, &carry, &strong, PLAYER_X, PLAYER_Y,
                                             &aim, err, sizeof(err)));
    dc_gpu_world_body_t world;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 0x80000001u, &world, err,
                                       sizeof(err)));
    float fast = sqrtf(powf((float)world.body.vx_fp / 65536.f, 2.f) +
                       powf((float)world.body.vy_fp / 65536.f, 2.f));
    fr_player_manipulation_reset(&carry);
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(setup_world());
    ASSERT_TRUE(spawn_and_settle(PLAYER_X + RETICLE_DX, PLAYER_Y + RETICLE_DY,
                                 4u, 4u) != 0u);
    fr_player_manipulation_t weak = manipulated(60.0, 1.0);
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &weak, PLAYER_X, PLAYER_Y,
                                            &on_body, err, sizeof(err)));
    ASSERT_TRUE(fr_player_manipulation_throw(gpu, &carry, &weak, PLAYER_X, PLAYER_Y,
                                             &aim, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 0x80000001u, &world, err,
                                       sizeof(err)));
    float slow = sqrtf(powf((float)world.body.vx_fp / 65536.f, 2.f) +
                       powf((float)world.body.vy_fp / 65536.f, 2.f));
    printf("throw speeds: strong %.2f weak %.2f\n", fast, slow);
    ASSERT_TRUE(fast > slow);
    fr_player_manipulation_reset(&carry);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_grab_refuses_without_a_body_at_the_reticle(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    fr_player_manipulation_t m = defaults();
    fr_player_point_t reticle = reticle_at(PLAYER_X + RETICLE_DX,
                                           PLAYER_Y + RETICLE_DY);
    ASSERT_TRUE(!fr_player_manipulation_grab(gpu, &carry, &m, PLAYER_X, PLAYER_Y,
                                             &reticle, err, sizeof(err)));
    fr_player_manipulation_reset(&carry);
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    printf("=== Player Manipulation Tests ===\n\n");
    RUN(test_grab_selects_body_at_reticle_within_reach);
    RUN(test_grab_refuses_overweight_bodies);
    RUN(test_grab_refuses_bodies_beyond_reach);
    RUN(test_grab_refuses_without_a_body_at_the_reticle);
    RUN(test_carried_body_tracks_the_carry_point);
    RUN(test_drop_releases_the_carry);
    RUN(test_throw_launches_toward_the_reticle_with_force);
    RUN(test_throw_force_attribute_scales_the_launch);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
