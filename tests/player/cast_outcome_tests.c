#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ferrum/player/abilities.h"
#include "ferrum/player/placement.h"
#include "ferrum/renderer/sprite_gpu.h"
#include "../../src/app/level_internal.h"
#include "../../src/app/player_session_internal.h"
#include "../../src/app/player_view.h"
#include "../../src/vulkan/runtime_body_gpu.h"

static int g_pass, g_fail;
static char error_buf[256];
static const char failure_message[] = "Injected Vulkan cast execution failure";
static const char blocked_message[] = "Fixture found a blocked cast footprint";
static bool backend_failed, backend_applied;
static unsigned bolt_calls, storm_calls, drive_calls, ray_steps, ray_emits;
static dc_level_view_t view;
static dc_body_storage_owner_t saved_owner;
static enum { JOIN_READY, JOIN_DRIVE_FAILS, JOIN_RAY_FAILS, JOIN_SPRITE_FAILS } join_fault;
static unsigned candidate_bodies, candidate_parcels, visible_poses, retired_drives, retired_rays;
static max_align_t existing_drive, next_drive, existing_ray, next_ray;
static bool test_admission;
static const char join_error[] = "Injected new-player setup failure";
static const char split_error[] = "Fixture drawable cannot fit a split layout";
static bool split_fits;
static uint32_t displayed_bands, controller_viewport;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s [%s]\n", __FILE__, __LINE__, #expr, error_buf); \
    ++g_fail; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

static fr_player_cast_result_t mock_cast(char *err, uint32_t cap) {
    if (backend_applied) return FR_PLAYER_CAST_APPLIED;
    snprintf(err, cap, "%s", backend_failed ? failure_message : blocked_message);
    return backend_failed ? FR_PLAYER_CAST_FAILED : FR_PLAYER_CAST_REFUSED;
}

fr_player_cast_result_t fr_player_try_cast_body_fire_bolt(dc_gpu_t *gpu, uint64_t owner, const dc_gpu_world_body_t *caster,
    const fr_player_point_t *target, uint32_t *body_id, char *err, uint32_t cap) {
    (void)gpu; (void)owner; (void)caster; (void)target; (void)body_id;
    ++bolt_calls;
    return mock_cast(err, cap);
}

fr_player_cast_result_t fr_player_try_cast_ice_storm(dc_gpu_t *gpu, const fr_player_point_t *target,
    uint32_t radius, char *err, uint32_t cap) {
    (void)gpu; (void)target; (void)radius;
    ++storm_calls;
    return mock_cast(err, cap);
}

bool dc_gpu_read_world_body(dc_gpu_t *gpu, uint32_t id, dc_gpu_world_body_t *body,
    char *err, uint32_t cap) {
    (void)gpu; (void)err; (void)cap;
    *body = (dc_gpu_world_body_t){.body = {.id = id, .active = true, .width = 40u, .height = 40u}};
    return true;
}

bool fr_player_drive_gpu_submit(fr_player_drive_gpu_t *drive, const fr_player_drive_input_t *inputs,
    uint32_t count, char *err, uint32_t cap) {
    (void)drive; (void)inputs; (void)count; (void)err; (void)cap;
    ++drive_calls;
    return true;
}

bool fr_renderer_sprite_prepare_poses(dc_gpu_t *gpu, const fr_renderer_sprite_t *poses,
    uint32_t count, char *err, uint32_t cap) {
    (void)gpu; (void)poses;
    if (count == 2u && join_fault == JOIN_SPRITE_FAILS) {
        snprintf(err, cap, "%s", join_error);
        return false;
    }
    return true;
}

void fr_renderer_sprite_commit_poses(dc_gpu_t *gpu, const fr_renderer_sprite_t *poses, uint32_t count) {
    (void)gpu; (void)poses;
    visible_poses = count;
}

bool fr_renderer_sprite_poses(dc_gpu_t *gpu, const fr_renderer_sprite_t *poses,
    uint32_t count, char *err, uint32_t cap) {
    if (!fr_renderer_sprite_prepare_poses(gpu, poses, count, err, cap)) return false;
    fr_renderer_sprite_commit_poses(gpu, poses, count);
    return true;
}

bool fr_player_manipulation_tick(dc_gpu_t *gpu, fr_player_carry_t *carry, const dc_gpu_world_body_t *player) {
    (void)gpu; (void)carry; (void)player;
    return true;
}

bool fr_player_manipulation_drop(dc_gpu_t *gpu, fr_player_carry_t *carry, char *err, uint32_t cap) {
    (void)gpu; (void)carry; (void)err; (void)cap;
    return false;
}

bool fr_player_manipulation_grab(dc_gpu_t *gpu, fr_player_carry_t *carry, const fr_player_manipulation_t *settings,
    const dc_gpu_world_body_t *player, const fr_player_point_t *target, char *err, uint32_t cap) {
    (void)gpu; (void)carry; (void)settings; (void)player; (void)target; (void)err; (void)cap;
    return false;
}

bool fr_player_manipulation_throw(dc_gpu_t *gpu, fr_player_carry_t *carry, const fr_player_manipulation_t *settings,
    const fr_player_point_t *target, char *err, uint32_t cap) {
    (void)gpu; (void)carry; (void)settings; (void)target; (void)err; (void)cap;
    return false;
}

bool fr_player_place_material(dc_gpu_t *gpu, fr_player_inventory_t *inventory, const fr_player_point_t *target,
    uint16_t material, bool rigid, bool *placed, char *err, uint32_t cap) {
    (void)gpu; (void)inventory; (void)target; (void)material; (void)rigid; (void)err; (void)cap;
    *placed = false;
    return false;
}

bool fr_player_harvest_tick(dc_gpu_t *gpu, fr_player_inventory_t *inventory, fr_player_chew_t *work,
    const fr_player_point_t *origin, const fr_player_point_t *target, bool held, uint64_t elapsed,
    bool *eligible, char *err, uint32_t cap) {
    (void)gpu; (void)inventory; (void)work; (void)origin; (void)target; (void)held;
    (void)elapsed; (void)eligible; (void)err; (void)cap;
    return false;
}

bool fr_fire_ray_gpu_submit(fr_fire_ray_gpu_t *ray, const fr_fire_ray_input_t *inputs,
    uint32_t count, float seconds, char *err, uint32_t cap) {
    (void)ray; (void)seconds; (void)err; (void)cap;
    ++ray_steps;
    for (uint32_t n = 0; n < count; ++n) ray_emits += inputs[n].emit;
    return true;
}

bool dc_player_view_aim(dc_level_view_t *view, uint32_t viewport, bool controller,
    const fr_input_state_t *input, fr_player_point_t *target) {
    (void)view; (void)input; (void)target;
    if (controller) controller_viewport = viewport;
    return false;
}

void dc_player_view_reticle(dc_level_view_t *level, uint32_t viewport, const fr_player_point_t *target) {
    (void)level; (void)viewport; (void)target;
}

void dc_player_view_follow(dc_level_view_t *view, const uint32_t *ids, uint32_t count) {
    (void)view; (void)ids; (void)count;
}

bool dc_level_view_accepts_event(const dc_level_view_t *level, const SDL_Event *event) {
    return level == &view && event;
}

uint32_t dc_gpu_body_capacity(const dc_gpu_t *gpu) {
    (void)gpu;
    return saved_owner.runtime_id ? 1u : 0u;
}

dc_body_storage_owner_t *dc_gpu_body_owner(const dc_gpu_t *gpu, uint32_t slot) {
    (void)gpu; (void)slot;
    return &saved_owner;
}

bool fr_beaver_body_admit_prepared(dc_gpu_t *gpu, uint64_t identity, uint64_t transaction, uint32_t slot,
    dc_chunk_coord_t spawn, int32_t x, int32_t y, fr_beaver_body_prepare_fn prepare, void *user,
    fr_beaver_body_t *body, bool *world_consistent, char *err, uint32_t cap) {
    (void)gpu; (void)transaction; (void)slot; (void)spawn; (void)x; (void)y;
    *world_consistent = true;
    if (!test_admission) return false;
    fr_beaver_body_t candidate = {.binding = {identity, 72u, 8u}, .width = 40u, .height = 40u};
    ++candidate_bodies;
    if (!prepare(user, &candidate, err, cap)) {
        --candidate_bodies;
        return false;
    }
    ++candidate_parcels;
    *body = candidate;
    return true;
}

bool fr_player_drive_gpu_create(dc_gpu_t *gpu, const fr_player_body_binding_t *bindings,
    uint32_t count, float speed, float jump, fr_player_drive_gpu_t **drive, char *err, uint32_t cap) {
    (void)gpu; (void)bindings; (void)count; (void)speed; (void)jump;
    if (!test_admission || join_fault == JOIN_DRIVE_FAILS) {
        snprintf(err, cap, "%s", join_error);
        return false;
    }
    *drive = (fr_player_drive_gpu_t *)&next_drive;
    return true;
}

void fr_player_drive_gpu_destroy(fr_player_drive_gpu_t *drive) {
    retired_drives += drive == (fr_player_drive_gpu_t *)&existing_drive;
}

bool fr_fire_ray_gpu_create(dc_gpu_t *gpu, const fr_player_body_binding_t *bindings,
    uint32_t count, fr_fire_ray_config_t config, fr_fire_ray_gpu_t **ray, char *err, uint32_t cap) {
    (void)gpu; (void)bindings; (void)count; (void)config;
    if (!test_admission || join_fault == JOIN_RAY_FAILS) {
        snprintf(err, cap, "%s", join_error);
        return false;
    }
    *ray = (fr_fire_ray_gpu_t *)&next_ray;
    return true;
}

void fr_fire_ray_gpu_destroy(fr_fire_ray_gpu_t *ray) {
    retired_rays += ray == (fr_fire_ray_gpu_t *)&existing_ray;
}

bool fr_fire_ray_gpu_prepare_emitter(fr_fire_ray_gpu_t *ray, const fr_player_body_binding_t *binding,
    char *err, uint32_t cap) {
    if (join_fault == JOIN_RAY_FAILS) {
        snprintf(err, cap, "%s", join_error);
        return false;
    }
    return ray == (fr_fire_ray_gpu_t *)&existing_ray && binding->identity == 2u;
}

void fr_fire_ray_gpu_commit_emitter(fr_fire_ray_gpu_t *ray, const fr_player_body_binding_t *binding) {
    (void)ray; (void)binding;
}

bool dc_gpu_set_split_presentation(dc_gpu_t *gpu, uint32_t count, const dc_presentation_focus_t *crops) {
    (void)gpu; (void)crops;
    if (count && !split_fits) return false;
    displayed_bands = count;
    return true;
}

bool dc_player_view_split(dc_level_view_t *level, const uint32_t *ids, uint32_t count, char *err, uint32_t cap) {
    (void)level; (void)ids;
    if (!split_fits) {
        if (err && cap) snprintf(err, cap, "%s", split_error);
        return false;
    }
    displayed_bands = count;
    return true;
}

static bool begin_cast(struct dc_player_session *s, bool ice) {
    *s = (struct dc_player_session){.count = 1u, .device = {41}, .aim_override = {true},
        .aim_point = {{{0, 0}, 56.f, 16.f}}, .bodies = {{.binding = {1u, 71u, 1u}, .width = 40u, .height = 40u}}};
    view.players = s;
    char *err = error_buf;
    err[0] = '\0';
    bolt_calls = storm_calls = drive_calls = ray_steps = ray_emits = 0u;
    backend_failed = backend_applied = false;
    fr_input_init(&s->input);
    if (!fr_player_registry_init(&s->registry, s->definitions, 2u) ||
        !fr_player_register(&s->registry, "test:fire_bolt", FR_PLAYER_ABILITY, 0, &s->fire, err, sizeof(error_buf)) ||
        !fr_player_register(&s->registry, "test:ice_storm", FR_PLAYER_ABILITY, 0, &s->ice, err, sizeof(error_buf)) ||
        !fr_player_registry_seal(&s->registry) ||
        !fr_player_init(&s->players[0], 1u, &s->registry, s->values[0], s->abilities[0], 2u)) return false;
    uint32_t spell = ice ? s->ice : s->fire;
    return fr_player_ability_set(&s->players[0], spell, true) &&
        fr_player_hotbar_put_spell(&s->hotbar[0], 1u, spell) &&
        fr_input_add_player(&s->input, 1u) && fr_input_connect(&s->input, 41, 1u) &&
        fr_input_bind(&s->input, 41, 4u, FR_INPUT_FIRE, 1.f) &&
        fr_input_event(&s->input, 41, 4u, 1.f, false) &&
        dc_players_step(&view, .25f, err, sizeof(error_buf));
}

static void check_live_cast(bool ice, bool fails) {
    struct dc_player_session session;
    char *err = error_buf;
    ASSERT_TRUE(begin_cast(&session, ice));
    backend_failed = fails;
    ASSERT_EQ(bolt_calls + storm_calls, 0u);

    bool continued = dc_players_step(&view, .15625f, err, sizeof(error_buf));
    printf("%s %s: continued=%d, drive submissions=%u, error=%s\n", ice ? "ice" : "bolt",
           fails ? "device failure" : "refusal", continued, drive_calls, err);
    ASSERT_EQ(bolt_calls, ice ? 0u : 1u);
    ASSERT_EQ(storm_calls, ice ? 1u : 0u);
    ASSERT_EQ(continued, !fails);
    ASSERT_EQ(drive_calls, fails ? 1u : 2u);
    ASSERT_EQ(strcmp(err, fails ? failure_message : ""), 0);
    err[0] = '\0';
    ASSERT_TRUE(dc_players_step(&view, .125f, err, sizeof(error_buf)));
    ASSERT_EQ(bolt_calls + storm_calls, 1u);
    PASS();
}

static void test_bolt_backend_failure_stops_the_step(void) { check_live_cast(false, true); }
static void test_storm_backend_failure_stops_the_step(void) { check_live_cast(true, true); }
static void test_bolt_refusal_keeps_the_step_healthy(void) { check_live_cast(false, false); }
static void test_storm_refusal_keeps_the_step_healthy(void) { check_live_cast(true, false); }

static void test_ui_round_trip_drops_old_windup_and_allows_a_fresh_press(void) {
    struct dc_player_session s;
    ASSERT_TRUE(begin_cast(&s, true));
    dc_players_ui(&view, true);
    dc_players_ui(&view, false);
    ASSERT_TRUE(dc_players_step(&view, .15625f, error_buf, sizeof(error_buf)));
    printf("UI round trip emitted %u old storms\n", storm_calls);
    ASSERT_EQ(storm_calls, 0u);
    ASSERT_EQ(s.players[0].action, FR_PLAYER_IDLE);

    ASSERT_TRUE(fr_input_event(&s.input, 41, 4u, 0.f, false));
    ASSERT_TRUE(fr_input_event(&s.input, 41, 4u, 1.f, false));
    ASSERT_TRUE(dc_players_step(&view, .25f, error_buf, sizeof(error_buf)));
    ASSERT_TRUE(dc_players_step(&view, .15625f, error_buf, sizeof(error_buf)));
    ASSERT_EQ(storm_calls, 1u);
    PASS();
}

static void test_unbound_controller_cannot_emit_its_latched_storm(void) {
    struct dc_player_session s;
    ASSERT_TRUE(begin_cast(&s, true));
    ASSERT_TRUE(fr_input_disconnect(&s.input, s.device[0]));
    s.device[0] = INT64_MIN;
    ASSERT_TRUE(dc_players_step(&view, .15625f, error_buf, sizeof(error_buf)));
    printf("Unbound controller emitted %u storms\n", storm_calls);
    ASSERT_EQ(storm_calls, 0u);
    ASSERT_EQ(s.players[0].action, FR_PLAYER_IDLE);
    PASS();
}

static void test_unfocused_input_cannot_start_a_ray(void) {
    struct dc_player_session s;
    ASSERT_TRUE(begin_cast(&s, false));
    s.ray = (fr_fire_ray_gpu_t *)&s;
    fr_input_focus(&s.input, false);
    ASSERT_TRUE(dc_players_step(&view, .15625f, error_buf, sizeof(error_buf)));
    printf("Unfocused ray step emitted %u rays\n", ray_emits);
    ASSERT_EQ(ray_steps, 1u);
    ASSERT_EQ(ray_emits, 0u);
    PASS();
}

static void test_ui_transition_preserves_emitted_cast_recovery(void) {
    struct dc_player_session s;
    ASSERT_TRUE(begin_cast(&s, true));
    backend_applied = true;
    ASSERT_TRUE(dc_players_step(&view, .15625f, error_buf, sizeof(error_buf)));
    ASSERT_EQ(storm_calls, 1u);
    ASSERT_EQ(s.players[0].action, FR_PLAYER_CAST);
    uint64_t recovery = s.players[0].elapsed_ms;
    dc_players_ui(&view, true);
    dc_players_ui(&view, false);
    ASSERT_EQ(s.players[0].elapsed_ms, recovery);
    ASSERT_TRUE(dc_players_step(&view, .08f, error_buf, sizeof(error_buf)));
    ASSERT_EQ(s.players[0].action, FR_PLAYER_CAST);
    ASSERT_TRUE(dc_players_step(&view, .20f, error_buf, sizeof(error_buf)));
    ASSERT_EQ(s.players[0].action, FR_PLAYER_IDLE);
    ASSERT_EQ(storm_calls, 1u);
    PASS();
}

static bool open_vacant_seat(struct dc_player_session *s) {
    saved_owner = (dc_body_storage_owner_t){0};
    *s = (struct dc_player_session){.count = 2u, .device = {FR_INPUT_KEYBOARD_DEVICE, INT64_MIN},
        .bodies = {{.binding = {1u, 71u, 1u}}, {.binding = {2u, 72u, 8u}}}};
    s->inventory[1].mass_fp[DC_ITEM_WOOD] = 39322u;
    view.players = s;
    error_buf[0] = '\0';
    fr_input_init(&s->input);
    return fr_input_add_player(&s->input, 1u) && fr_input_add_player(&s->input, 2u) &&
        fr_input_connect(&s->input, FR_INPUT_KEYBOARD_DEVICE, 1u);
}

static void test_direct_rejoin_uses_the_vacant_owner_and_keeps_its_ui_context(void) {
    struct dc_player_session s;
    ASSERT_TRUE(open_vacant_seat(&s));
    ASSERT_TRUE(fr_input_context(&s.input, 2u, FR_INPUT_UI));
    int64_t device = INT64_C(1) << 40;
    ASSERT_TRUE(dc_players_join(&view, device, error_buf, sizeof(error_buf)));
    ASSERT_TRUE(dc_players_join(&view, device, error_buf, sizeof(error_buf)));
    ASSERT_EQ(s.count, 2u);
    ASSERT_EQ(s.device[1], device);
    ASSERT_EQ(s.bodies[1].binding.body_id, 72u);
    ASSERT_EQ(s.bodies[1].binding.generation, 8u);
    ASSERT_EQ(s.inventory[1].mass_fp[DC_ITEM_WOOD], 39322u);
    ASSERT_EQ(s.input.players[1].context, FR_INPUT_UI);
    ASSERT_TRUE(fr_input_event(&s.input, device, SDL_CONTROLLER_BUTTON_Y, 1.f, false));
    fr_input_state_t sample;
    ASSERT_TRUE(fr_input_sample(&s.input, 2u, &sample));
    ASSERT_EQ(sample.pressed, 0u);
    PASS();
}

static void test_start_binding_failure_leaves_the_seat_vacant_and_retryable(void) {
    struct dc_player_session s;
    ASSERT_TRUE(open_vacant_seat(&s));
    ASSERT_TRUE(fr_input_connect(&s.input, 99, 1u));
    for (uint32_t n = 0; n < FR_INPUT_BINDINGS; ++n)
        ASSERT_TRUE(fr_input_bind(&s.input, 99, 0xb000u + n, FR_INPUT_MOVE, 1.f));
    fr_input_t before = s.input;
    SDL_Event pad = {.cbutton = {.type = SDL_CONTROLLERBUTTONDOWN, .which = 43,
        .button = SDL_CONTROLLER_BUTTON_START}};
    ASSERT_TRUE(dc_players_event(&view, &pad));
    printf("Failed Start reserved device %lld, expected vacant sentinel\n", (long long)s.device[1]);
    ASSERT_EQ(s.device[1], INT64_MIN);
    ASSERT_EQ(memcmp(&before, &s.input, sizeof(before)), 0);
    ASSERT_EQ(s.failure[0], '\0');

    ASSERT_TRUE(fr_input_disconnect(&s.input, 99));
    ASSERT_TRUE(dc_players_event(&view, &pad));
    ASSERT_EQ(s.device[1], 43);
    pad.cbutton.button = SDL_CONTROLLER_BUTTON_Y;
    ASSERT_TRUE(dc_players_event(&view, &pad));
    fr_input_state_t sample;
    ASSERT_TRUE(fr_input_sample(&s.input, 2u, &sample));
    ASSERT_EQ(sample.pressed, 1u << FR_INPUT_ICE);
    ASSERT_EQ(s.inventory[1].mass_fp[DC_ITEM_WOOD], 39322u);
    PASS();
}

static void test_reserved_input_ids_never_admit_or_change_a_player(void) {
    struct dc_player_session s;
    ASSERT_TRUE(open_vacant_seat(&s));
    ASSERT_TRUE(!dc_players_join(&view, INT64_MIN, error_buf, sizeof(error_buf)));

    ASSERT_TRUE(begin_cast(&s, true));
    ASSERT_TRUE(fr_input_connect(&s.input, FR_INPUT_KEYBOARD_DEVICE, 1u));
    uint64_t transaction = DC_PLAYER_TRANSACTION_BASE + 2u;
    saved_owner = (dc_body_storage_owner_t){.state = DC_BODY_OWNER_LIVE, .runtime_id = 72u,
        .generation = 8u, .source_transaction = {(uint32_t)transaction, (uint32_t)(transaction >> 32u)}};
    fr_input_t before = s.input;
    ASSERT_TRUE(!dc_players_join(&view, FR_INPUT_KEYBOARD_DEVICE, error_buf, sizeof(error_buf)));
    ASSERT_EQ(s.count, 1u);
    ASSERT_EQ(s.bodies[1].binding.body_id, 0u);
    ASSERT_EQ(memcmp(&before, &s.input, sizeof(before)), 0);
    PASS();
}

static void check_join_rolls_back(unsigned failure) {
    struct dc_player_session s;
    ASSERT_TRUE(begin_cast(&s, true));
    saved_owner = (dc_body_storage_owner_t){0};
    dc_chunk_slot_t slot = {.coord = {-1, -1}, .state = DC_SLOT_ACTIVE};
    view.table = (dc_chunk_table_t){.slots = &slot, .capacity = 1u};
    s.drive = (fr_player_drive_gpu_t *)&existing_drive;
    s.ray = (fr_fire_ray_gpu_t *)&existing_ray;
    s.inventory[1].mass_fp[DC_ITEM_WOOD] = 39322u;
    fr_input_t input_before = s.input;
    test_admission = true;
    candidate_bodies = candidate_parcels = retired_drives = retired_rays = 0u;
    join_fault = failure;

    bool joined = dc_players_join(&view, 43, error_buf, sizeof(error_buf));
    printf("Join fault %u leaves %u seats, %u candidate bodies and %u parcels\n",
           failure, s.count, candidate_bodies, candidate_parcels);
    ASSERT_TRUE(!joined);
    ASSERT_EQ(strcmp(error_buf, join_error), 0);
    ASSERT_EQ(s.count, 1u);
    ASSERT_TRUE(!slot.dirty);
    ASSERT_EQ(s.bodies[1].binding.body_id, 0u);
    ASSERT_EQ(candidate_bodies, 0u);
    ASSERT_EQ(candidate_parcels, 0u);
    ASSERT_EQ(visible_poses, 1u);
    ASSERT_EQ(memcmp(&s.input, &input_before, sizeof(s.input)), 0);
    ASSERT_EQ(s.drive, (fr_player_drive_gpu_t *)&existing_drive);
    ASSERT_EQ(s.ray, (fr_fire_ray_gpu_t *)&existing_ray);
    ASSERT_EQ(retired_drives + retired_rays, 0u);

    ASSERT_TRUE(dc_players_step(&view, .05f, error_buf, sizeof(error_buf)));
    join_fault = JOIN_READY;
    ASSERT_TRUE(dc_players_join(&view, 43, error_buf, sizeof(error_buf)));
    ASSERT_EQ(s.count, 2u);
    ASSERT_EQ(candidate_bodies, 1u);
    ASSERT_EQ(candidate_parcels, 1u);
    ASSERT_EQ(visible_poses, 2u);
    ASSERT_EQ(s.ray, (fr_fire_ray_gpu_t *)&existing_ray);
    ASSERT_EQ(s.inventory[1].mass_fp[DC_ITEM_WOOD], 39322u);
    PASS();
}

static void test_failed_drive_preparation_rolls_join_back(void) { check_join_rolls_back(JOIN_DRIVE_FAILS); }
static void test_failed_ray_preparation_rolls_join_back(void) { check_join_rolls_back(JOIN_RAY_FAILS); }
static void test_failed_sprite_preparation_rolls_join_back(void) { check_join_rolls_back(JOIN_SPRITE_FAILS); }

static bool start_second_seat(struct dc_player_session *session, dc_chunk_slot_t *slot) {
    if (!begin_cast(session, false)) return false;
    *slot = (dc_chunk_slot_t){.coord = {-1, -1}, .state = DC_SLOT_ACTIVE};
    view.table = (dc_chunk_table_t){.slots = slot, .capacity = 1u};
    saved_owner = (dc_body_storage_owner_t){0};
    join_fault = JOIN_READY;
    test_admission = true;
    displayed_bands = 0u;
    const SDL_Event start = {.cbutton = {.type = SDL_CONTROLLERBUTTONDOWN, .which = 43,
        .button = SDL_CONTROLLER_BUTTON_START}};
    return dc_players_event(&view, &start);
}

static void test_start_recovers_split_without_rejoining_after_layout_rejection(void) {
    struct dc_player_session session;
    dc_chunk_slot_t slot;
    split_fits = false;
    ASSERT_TRUE(start_second_seat(&session, &slot));
    ASSERT_EQ(session.count, 2u);
    ASSERT_EQ(displayed_bands, 0u);
    printf("Rejected Start layout with %u seats sets failure to '%s'\n", session.count, session.failure);
    ASSERT_TRUE(dc_players_step(&view, .05f, error_buf, sizeof(error_buf)));
    ASSERT_TRUE(dc_players_present(&view, error_buf, sizeof(error_buf)));
    uint32_t body = session.bodies[1].binding.body_id;
    split_fits = true;
    ASSERT_TRUE(dc_players_present(&view, error_buf, sizeof(error_buf)));
    ASSERT_EQ(displayed_bands, 2u);
    ASSERT_EQ(session.bodies[1].binding.body_id, body);
    PASS();
}

static void test_rejected_split_refresh_falls_back_and_rebinds_aim_on_recovery(void) {
    struct dc_player_session session;
    dc_chunk_slot_t slot;
    split_fits = true;
    ASSERT_TRUE(start_second_seat(&session, &slot));
    ASSERT_EQ(displayed_bands, 2u);
    split_fits = false;
    const char previous_error[] = "Earlier fixture diagnostic";
    snprintf(error_buf, sizeof(error_buf), "%s", previous_error);
    bool refreshed = dc_players_present(&view, error_buf, sizeof(error_buf));
    printf("Rejected active layout refresh returned %d with %u bands\n", refreshed, displayed_bands);
    ASSERT_TRUE(refreshed);
    ASSERT_EQ(strcmp(error_buf, previous_error), 0);
    ASSERT_EQ(displayed_bands, 0u);
    controller_viewport = UINT32_MAX;
    ASSERT_TRUE(dc_players_step(&view, .05f, error_buf, sizeof(error_buf)));
    ASSERT_EQ(controller_viewport, 0u);
    split_fits = true;
    ASSERT_TRUE(dc_players_present(&view, error_buf, sizeof(error_buf)));
    ASSERT_EQ(displayed_bands, 2u);
    ASSERT_TRUE(dc_players_step(&view, .05f, error_buf, sizeof(error_buf)));
    ASSERT_EQ(controller_viewport, 1u);
    ASSERT_EQ(session.device[1], 43);
    split_fits = false;
    ASSERT_TRUE(dc_players_present(&view, error_buf, sizeof(error_buf)));
    ASSERT_TRUE(dc_players_split_screen(&view, 0u, error_buf, sizeof(error_buf)));
    split_fits = true;
    ASSERT_TRUE(dc_players_present(&view, error_buf, sizeof(error_buf)));
    ASSERT_EQ(displayed_bands, 0u);
    PASS();
}

int main(void) {
    RUN(test_bolt_backend_failure_stops_the_step);
    RUN(test_storm_backend_failure_stops_the_step);
    RUN(test_bolt_refusal_keeps_the_step_healthy);
    RUN(test_storm_refusal_keeps_the_step_healthy);
    RUN(test_ui_round_trip_drops_old_windup_and_allows_a_fresh_press);
    RUN(test_unbound_controller_cannot_emit_its_latched_storm);
    RUN(test_unfocused_input_cannot_start_a_ray);
    RUN(test_ui_transition_preserves_emitted_cast_recovery);
    RUN(test_direct_rejoin_uses_the_vacant_owner_and_keeps_its_ui_context);
    RUN(test_start_binding_failure_leaves_the_seat_vacant_and_retryable);
    RUN(test_reserved_input_ids_never_admit_or_change_a_player);
    RUN(test_failed_drive_preparation_rolls_join_back);
    RUN(test_failed_ray_preparation_rolls_join_back);
    RUN(test_failed_sprite_preparation_rolls_join_back);
    RUN(test_start_recovers_split_without_rejoining_after_layout_rejection);
    RUN(test_rejected_split_refresh_falls_back_and_rebinds_aim_on_recovery);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
