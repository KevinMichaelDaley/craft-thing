#include "../../src/app/player_session.c"

static int g_pass, g_fail;
static char error_buf[256];
static const char failure_message[] = "Injected Vulkan cast execution failure";
static const char blocked_message[] = "Fixture found a blocked cast footprint";
static bool backend_failed, backend_applied;
static unsigned bolt_calls, storm_calls, drive_calls, ray_steps, ray_emits;
static dc_level_view_t view;

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

bool fr_renderer_sprite_poses(dc_gpu_t *gpu, const fr_renderer_sprite_t *poses,
    uint32_t count, char *err, uint32_t cap) {
    (void)gpu; (void)poses; (void)count; (void)err; (void)cap;
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
    (void)view; (void)viewport; (void)controller; (void)input; (void)target;
    return false;
}

void dc_player_view_follow(dc_level_view_t *view, const uint32_t *ids, uint32_t count) {
    (void)view; (void)ids; (void)count;
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

static void test_focus_context_change_preserves_already_emitted_recovery(void) {
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

int main(void) {
    RUN(test_bolt_backend_failure_stops_the_step);
    RUN(test_storm_backend_failure_stops_the_step);
    RUN(test_bolt_refusal_keeps_the_step_healthy);
    RUN(test_storm_refusal_keeps_the_step_healthy);
    RUN(test_ui_round_trip_drops_old_windup_and_allows_a_fresh_press);
    RUN(test_unbound_controller_cannot_emit_its_latched_storm);
    RUN(test_unfocused_input_cannot_start_a_ray);
    RUN(test_focus_context_change_preserves_already_emitted_recovery);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
