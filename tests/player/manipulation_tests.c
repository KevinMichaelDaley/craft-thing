#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/gpu.h"
#include "../../src/vulkan/gpu_internal.h"
#include "ferrum/player/manipulation.h"

static int g_pass, g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();} while (0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

/* Player anchor on the ground; reticle and carry point derived from it. */
#define PLAYER_X 200.f
#define PLAYER_Y 200.f
#define RETICLE_DX 12.f
#define RETICLE_DY -6.f

static dc_gpu_t *gpu = NULL;
static char err[256];
static const dc_gpu_world_body_t player_anchor = {
    .chunk = {3, 3}, .body = {.x_fp = 8 * DC_FLUID_FULL, .y_fp = 8 * DC_FLUID_FULL}
};

static void close_world(void) {
    dc_gpu_destroy(gpu);
    gpu = NULL;
}

static bool setup_world(void) {
    err[0] = 0;
    if (!gpu && !dc_gpu_create(&gpu, 480, 270,
                       "build/shaders/pattern.comp.spv", err, sizeof(err)))
        return false;
    for (uint32_t slot = 0; slot < dc_gpu_body_capacity(gpu); ++slot) {
        const dc_gpu_body_record_t *body = dc_gpu_body_record(gpu, slot);
        if (body->body.active && !dc_gpu_remove_body(gpu, body->body.id, err, sizeof(err)))
            return false;
    }
    if (!dc_gpu_set_page(gpu, 3, 2, UINT32_MAX, err, sizeof(err)) ||
        !dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)) ||
        !dc_gpu_set_rigid_solver(gpu, false)) return false;
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
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &m, &player_anchor,
                                            &reticle, err, sizeof(err)));
    ASSERT_TRUE(fr_player_manipulation_carrying(&carry, id));
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
    ASSERT_TRUE(!fr_player_manipulation_grab(gpu, &carry, &m, &player_anchor,
                                             &reticle, err, sizeof(err)));
    ASSERT_TRUE(!fr_player_manipulation_carrying(&carry, id));
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
    ASSERT_TRUE(!fr_player_manipulation_grab(gpu, &carry, &m, &player_anchor,
                                             &reticle, err, sizeof(err)));
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
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &m, &player_anchor,
                                            &reticle, err, sizeof(err)));
    /* Two ticks of carry: the body rides the carry point over the player. */
    for (uint32_t tick = 0u; tick < 2u; tick++)
        ASSERT_TRUE(fr_player_manipulation_tick(gpu, &carry, &player_anchor));
    dc_gpu_world_body_t body;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, id, &body, err, sizeof(err)));
    ASSERT_EQ(body.chunk.x, player_anchor.chunk.x);
    ASSERT_EQ(body.chunk.y, player_anchor.chunk.y);
    ASSERT_EQ(body.body.x_fp, player_anchor.body.x_fp);
    ASSERT_EQ(body.body.y_fp, 0);
    PASS();
}
static void test_drop_releases_the_carry(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    uint32_t id = spawn_and_settle(PLAYER_X + RETICLE_DX, PLAYER_Y + RETICLE_DY, 4u,
                             4u);
    ASSERT_TRUE(id != 0u);
    fr_player_manipulation_t m = defaults();
    ASSERT_TRUE(dc_gpu_set_body_motion(gpu,id,.2f,0.f,DC_GPU_BODY_LOCK_ROTATION,err,sizeof(err)));
    fr_player_point_t reticle = reticle_at(PLAYER_X + RETICLE_DX + 1.f,
                                           PLAYER_Y + RETICLE_DY + 1.f);
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &m, &player_anchor,
                                            &reticle, err, sizeof(err)));
    ASSERT_TRUE(fr_player_manipulation_drop(gpu, &carry, err, sizeof(err)));
    ASSERT_TRUE(!fr_player_manipulation_carrying(&carry, id));
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,id,&motion,err,sizeof(err)));
    ASSERT_TRUE(fabsf(motion.angle-.2f)<.0001f);
    ASSERT_EQ(motion.flags & (DC_GPU_BODY_KINEMATIC|DC_GPU_BODY_LOCK_ROTATION),
              DC_GPU_BODY_LOCK_ROTATION);
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
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &m, &player_anchor,
                                            &on_body, err, sizeof(err)));
    ASSERT_TRUE(fr_player_manipulation_throw(gpu, &carry, &m,
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
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &strong, &player_anchor,
                                            &on_body, err, sizeof(err)));
    ASSERT_TRUE(fr_player_manipulation_throw(gpu, &carry, &strong,
                                             &aim, err, sizeof(err)));
    dc_gpu_world_body_t world;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 0x80000001u, &world, err,
                                       sizeof(err)));
    float fast = sqrtf(powf((float)world.body.vx_fp / 65536.f, 2.f) +
                       powf((float)world.body.vy_fp / 65536.f, 2.f));
    ASSERT_TRUE(setup_world());
    ASSERT_TRUE(spawn_and_settle(PLAYER_X + RETICLE_DX, PLAYER_Y + RETICLE_DY,
                                 4u, 4u) != 0u);
    fr_player_manipulation_t weak = manipulated(60.0, 1.0);
    ASSERT_TRUE(fr_player_manipulation_grab(gpu, &carry, &weak, &player_anchor,
                                            &on_body, err, sizeof(err)));
    ASSERT_TRUE(fr_player_manipulation_throw(gpu, &carry, &weak,
                                             &aim, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 0x80000001u, &world, err,
                                       sizeof(err)));
    float slow = sqrtf(powf((float)world.body.vx_fp / 65536.f, 2.f) +
                       powf((float)world.body.vy_fp / 65536.f, 2.f));
    printf("throw speeds: strong %.2f weak %.2f\n", fast, slow);
    ASSERT_TRUE(fast > slow);
    PASS();
}
static void test_grab_refuses_without_a_body_at_the_reticle(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    fr_player_manipulation_t m = defaults();
    fr_player_point_t reticle = reticle_at(PLAYER_X + RETICLE_DX,
                                           PLAYER_Y + RETICLE_DY);
    ASSERT_TRUE(!fr_player_manipulation_grab(gpu, &carry, &m, &player_anchor,
                                             &reticle, err, sizeof(err)));
    PASS();
}

static bool spawn_at(uint32_t id, fr_player_point_t point) {
    dc_gpu_world_body_t body = {.chunk = point.chunk, .body = {
        .x_fp = (int32_t)lroundf(point.x * DC_FLUID_FULL),
        .y_fp = (int32_t)lroundf(point.y * DC_FLUID_FULL),
        .width = 4u, .height = 4u, .id = id, .active = 1u}};
    return dc_gpu_spawn_world_body(gpu, body, err, sizeof(err)) &&
           dc_gpu_set_body_origin(gpu, gpu->body_origin, err, sizeof(err));
}

static dc_gpu_world_body_t actor_at(fr_player_point_t point) {
    return (dc_gpu_world_body_t){.chunk = point.chunk, .body = {
        .x_fp = (int32_t)lroundf(point.x * DC_FLUID_FULL),
        .y_fp = (int32_t)lroundf(point.y * DC_FLUID_FULL)}};
}

static bool grab_at(fr_player_carry_t *carry, fr_player_point_t player,
                     const fr_player_point_t *aim) {
    fr_player_manipulation_t settings = defaults();
    dc_gpu_world_body_t actor = actor_at(player);
    return fr_player_manipulation_grab(gpu, carry, &settings, &actor, aim, err, sizeof(err));
}

static void test_grab_picks_a_live_body_outside_the_legacy_id_range(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    ASSERT_TRUE(spawn_at(17u, reticle_at(212.f, 194.f)));
    fr_player_point_t solid = reticle_at(215.f, 196.f);
    ASSERT_TRUE(grab_at(&carry, reticle_at(200.f, 200.f), &solid));
    ASSERT_TRUE(fr_player_manipulation_carrying(&carry, 17u));
    PASS();
}

static void test_grab_rejects_air_just_outside_the_body(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t carry = {0};
    ASSERT_TRUE(spawn_at(0x80000001u, reticle_at(212.f, 194.f)));
    fr_player_point_t air = reticle_at(211.f, 195.f);
    ASSERT_TRUE(!grab_at(&carry, reticle_at(200.f, 200.f), &air));
    PASS();
}

static void test_a_second_player_cannot_claim_a_held_body(void) {
    ASSERT_TRUE(setup_world());
    fr_player_carry_t first = {0}, second = {0};
    const uint32_t id = 0x80000001u;
    fr_player_point_t start = reticle_at(212.f, 194.f);
    ASSERT_TRUE(spawn_at(id, start));
    fr_player_point_t aim = reticle_at(213.f, 195.f);
    ASSERT_TRUE(grab_at(&first, reticle_at(200.f, 200.f), &aim));
    ASSERT_TRUE(!grab_at(&second, reticle_at(225.f, 200.f), &aim));
    ASSERT_TRUE(fr_player_manipulation_drop(gpu, &first, err, sizeof(err)));
    ASSERT_TRUE(grab_at(&second, reticle_at(225.f, 200.f), &aim));
    ASSERT_TRUE(dc_gpu_remove_body(gpu,id,err,sizeof(err)));
    ASSERT_TRUE(spawn_at(id,start));
    ASSERT_TRUE(grab_at(&first, reticle_at(200.f, 200.f), &aim));
    dc_gpu_world_body_t other_player = actor_at(reticle_at(225.f, 200.f));
    ASSERT_TRUE(fr_player_manipulation_tick(gpu,&second,&other_player));
    ASSERT_TRUE(!fr_player_manipulation_carrying(&second,id));
    ASSERT_TRUE(fr_player_manipulation_carrying(&first,id));
    dc_gpu_world_body_t replacement;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu,id,&replacement,err,sizeof(err)));
    ASSERT_EQ(replacement.body.x_fp,(int32_t)(start.x*DC_FLUID_FULL));
    ASSERT_EQ(replacement.body.y_fp,(int32_t)(start.y*DC_FLUID_FULL));
    PASS();
}

static bool map_signed_seam(dc_chunk_coord_t *chunk) {
    const int64_t far = INT64_C(1) << 36;
    dc_chunk_coord_t origin = {far, -far};
    if (!dc_gpu_set_body_origin(gpu, origin, err, sizeof(err))) return false;
    dc_chunk_t *terrain = calloc(1, sizeof(*terrain));
    if (!terrain) return false;
    *chunk = (dc_chunk_coord_t){origin.x + 3, origin.y + 3};
    terrain->coord = *chunk;
    bool okay = dc_gpu_upload_chunk(gpu, 11u, terrain, err, sizeof(err));
    --terrain->coord.y;
    okay = okay && dc_gpu_upload_chunk(gpu, 10u, terrain, err, sizeof(err)) &&
           dc_gpu_set_page(gpu, 3u, 2u, 10u, err, sizeof(err));
    free(terrain);
    return okay;
}

static void test_far_carry_normalizes_the_destination_across_a_signed_seam(void) {
    ASSERT_TRUE(setup_world());
    ASSERT_TRUE(dc_gpu_set_rigid_solver(gpu, true));
    dc_chunk_coord_t chunk;
    ASSERT_TRUE(map_signed_seam(&chunk));
    fr_player_point_t rock = {chunk, 13.f, 9.f}, player = {chunk, 8.25f, 4.5f};
    ASSERT_TRUE(spawn_at(0x80000001u, rock));
    fr_player_carry_t carry = {0};
    ASSERT_TRUE(grab_at(&carry, player, &rock));
    dc_gpu_world_body_t actor = actor_at(player);
    ASSERT_TRUE(fr_player_manipulation_tick(gpu, &carry, &actor));
    dc_gpu_world_body_t held;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 0x80000001u, &held, err, sizeof(err)));
    ASSERT_EQ(held.chunk.x, chunk.x);
    ASSERT_EQ(held.chunk.y, chunk.y - 1);
    ASSERT_EQ(held.body.x_fp, 33 * DC_FLUID_FULL / 4);
    ASSERT_EQ(held.body.y_fp, 121 * DC_FLUID_FULL / 2);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 0x80000001u, &held, err, sizeof(err)));
    ASSERT_EQ(held.chunk.y, chunk.y - 1);
    ASSERT_EQ(held.body.y_fp, 121 * DC_FLUID_FULL / 2);
    PASS();
}

static void test_far_throw_keeps_the_local_aim_direction(void) {
    ASSERT_TRUE(setup_world());
    dc_chunk_coord_t chunk;
    ASSERT_TRUE(map_signed_seam(&chunk));
    fr_player_point_t rock = {chunk, 13.f, 9.f}, player = {chunk, 8.f, 4.f};
    fr_player_point_t left = {chunk, 7.f, 9.f};
    ASSERT_TRUE(spawn_at(0x80000001u, rock));
    fr_player_carry_t carry = {0};
    ASSERT_TRUE(grab_at(&carry, player, &rock));
    fr_player_manipulation_t settings = defaults();
    ASSERT_TRUE(fr_player_manipulation_throw(gpu, &carry, &settings,
                                             &left, err, sizeof(err)));
    dc_gpu_world_body_t launched;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 0x80000001u, &launched, err, sizeof(err)));
    ASSERT_EQ(launched.body.vx_fp, -4 * (int32_t)DC_FLUID_FULL);
    ASSERT_EQ(launched.body.vy_fp, 0);
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
    RUN(test_grab_picks_a_live_body_outside_the_legacy_id_range);
    RUN(test_grab_rejects_air_just_outside_the_body);
    RUN(test_a_second_player_cannot_claim_a_held_body);
    RUN(test_far_carry_normalizes_the_destination_across_a_signed_seam);
    RUN(test_far_throw_keeps_the_local_aim_direction);
    close_world();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
