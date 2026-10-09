#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/app/level.h"
#include "../../src/app/view_config.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_quarter_native_window_keeps_both_spawned_boxes_visible(void) {
    char directory[] = "build/ui_multi_rigid_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_spawn_body(view, 63, 2, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_spawn_body(view, 122, 2, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 20u; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    uint32_t first = 0, second = 0, old = 0;
    ASSERT_TRUE(dc_level_view_pixel(view, 83, 52, &first, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 142, 52, &second, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 63, 2, &old, err, sizeof(err)));
    bool rendered = first == 0xff30c040u && second == 0xff30c040u &&
                    old != 0xff30c040u;
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    printf("two-box window pixels: %08x %08x, old footprint %08x\n", first, second, old);
    ASSERT_TRUE(rendered);
    PASS();
}

static void test_world_boxes_survive_camera_eviction_and_session_reload(void) {
    char directory[] = "build/ui_world_rigid_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_spawn_body(view, 63, 2, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    uint32_t color;
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &color, err, sizeof(err)));
    ASSERT_EQ(color, 0xff30c040u);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 16 * DC_CHUNK_SIDE, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    bool evicted = !dc_level_view_has_chunk(view, (dc_chunk_coord_t){0, INITIAL_CHUNK_Y});
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &color, err, sizeof(err)));
    bool clipped = color != 0xff30c040u;
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -16 * (int32_t)DC_CHUNK_SIDE, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &color, err, sizeof(err)));
    bool restored = color == 0xff30c040u;
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    ASSERT_TRUE(evicted && clipped && restored);
    view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &color, err, sizeof(err)));
    restored = color == 0xff30c040u;
    ASSERT_TRUE(dc_level_view_spawn_body(view, 122, 2, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    uint32_t second;
    ASSERT_TRUE(dc_level_view_pixel(view, 65, 2, &color, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 123, 2, &second, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    ASSERT_TRUE(restored);
    ASSERT_EQ(color, 0xff30c040u); ASSERT_EQ(second, 0xff30c040u);
    PASS();
}

static void test_streamed_window_broadphase_preserves_dense_pair_ids(void) {
    char directory[] = "build/ui_broadphase_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    for (uint32_t i = 0; i < 8; ++i)
        ASSERT_TRUE(dc_level_view_spawn_body(view, 63, 2, err, sizeof(err)));
    /* This tests streamed candidate identity while the overlapping fixture is paused. */
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    dc_gpu_broadphase_stats_t stats;
    dc_gpu_broadphase_pair_t pairs[256];
    ASSERT_TRUE(dc_level_view_broadphase(view, &stats, pairs, 256, err, sizeof(err)));
    uint32_t count = 0;
    for (uint32_t i = 0; i < stats.count; ++i) count += pairs[i].kind == DC_GPU_PAIR_BODY;
    bool complete = stats.active_bodies == 8 && !stats.overflow && count == 28;
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 16 * DC_CHUNK_SIDE, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_broadphase(view, &stats, pairs, 256, err, sizeof(err)));
    complete = complete && stats.active_bodies == 0 && stats.count == 0;
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -16 * (int32_t)DC_CHUNK_SIDE, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_broadphase(view, &stats, pairs, 256, err, sizeof(err)));
    bool seen[9][9] = {{false}};
    count = 0;
    for (uint32_t i = 0; i < stats.count; ++i) {
        if (pairs[i].kind != DC_GPU_PAIR_BODY) continue;
        uint32_t a = pairs[i].body_a, b = pairs[i].body_b;
        if (a < 1 || a >= b || b > 8 || seen[a][b]) { complete = false; continue; }
        seen[a][b] = true; ++count;
    }
    complete = complete && stats.active_bodies == 8 && !stats.overflow && count == 28;
    uint32_t pixel = 0;
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &pixel, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    ASSERT_EQ(pixel, 0xff30c040u);
    ASSERT_TRUE(complete);
    PASS();
}

static void test_convex_window_shapes_survive_eviction_and_reload(void) {
    char directory[] = "build/ui_convex_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    dc_gpu_body_shape_t shape = { .count = 4, .material = DC_GPU_BODY_STONE,
        .vertices = {{2 << 16, 0}, {4 << 16, 2 << 16}, {2 << 16, 4 << 16}, {0, 2 << 16}} };
    ASSERT_TRUE(dc_level_view_spawn_convex_body(view, 63, 2, 4, 4, &shape, err, sizeof(err)));
    shape.material = DC_GPU_BODY_WOOD;
    ASSERT_TRUE(dc_level_view_spawn_convex_body(view, 123, 2, 4, 4, &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    uint32_t corner, edge, wood;
    ASSERT_TRUE(dc_level_view_pixel(view, 63, 2, &corner, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &edge, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 124, 2, &wood, err, sizeof(err)));
    bool correct = corner != 0xff30c040u && edge == 0xff30c040u && wood == 0xff30c040u;
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 16 * DC_CHUNK_SIDE, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -16 * (int32_t)DC_CHUNK_SIDE, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 63, 2, &corner, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &edge, err, sizeof(err)));
    correct = correct && corner != 0xff30c040u && edge == 0xff30c040u;
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    view = dc_level_view_create(directory, 314, err, sizeof(err)); ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 63, 2, &corner, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &edge, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 124, 2, &wood, err, sizeof(err)));
    correct = correct && corner != 0xff30c040u && edge == 0xff30c040u && wood == 0xff30c040u;
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    ASSERT_TRUE(correct); PASS();
}

static void test_world_contacts_survive_camera_eviction_and_session_reload(void) {
    char directory[] = "build/ui_contacts_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err)); ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    dc_gpu_body_shape_t shape = { .count = 4, .material = DC_GPU_BODY_STONE,
        .vertices = {{2 << 16, 0}, {4 << 16, 2 << 16}, {2 << 16, 4 << 16}, {0, 2 << 16}} };
    ASSERT_TRUE(dc_level_view_spawn_convex_body(view, 63, 2, 4, 4, &shape, err, sizeof(err)));
    shape.material = DC_GPU_BODY_WOOD;
    ASSERT_TRUE(dc_level_view_spawn_convex_body(view, 64, 2, 4, 4, &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    dc_gpu_contact_stats_t stats; dc_gpu_contact_t before[16], after[16];
    ASSERT_TRUE(dc_level_view_contacts(view, &stats, before, 16, err, sizeof(err)));
    ASSERT_EQ(stats.count, 1u); ASSERT_EQ(stats.overflow, 0u);
    ASSERT_EQ(before[0].kind, DC_GPU_CONTACT_BODY);
    ASSERT_EQ(before[0].material_a, DC_GPU_BODY_STONE); ASSERT_EQ(before[0].material_b, DC_GPU_BODY_WOOD);
    ASSERT_TRUE(before[0].depth > 0);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 16 * DC_CHUNK_SIDE, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_contacts(view, &stats, after, 16, err, sizeof(err)));
    ASSERT_EQ(stats.count, 0u);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -16 * (int32_t)DC_CHUNK_SIDE, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_contacts(view, &stats, after, 16, err, sizeof(err)));
    ASSERT_EQ(stats.count, 1u); ASSERT_EQ(memcmp(before, after, sizeof(*before)), 0);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    view = dc_level_view_create(directory, 314, err, sizeof(err)); ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_contacts(view, &stats, after, 16, err, sizeof(err)));
    ASSERT_EQ(stats.count, 1u); ASSERT_EQ(memcmp(before, after, sizeof(*before)), 0);
    uint32_t pixel;
    ASSERT_TRUE(dc_level_view_pixel(view, 65, 3, &pixel, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    ASSERT_EQ(pixel, 0xff30c040u); PASS();
}

static void test_quarter_native_gpu_stack_settles_and_persists(void) {
    char directory[]="build/ui_xpbd_XXXXXX",err[256]={0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view,false));
    dc_gpu_rigid_solver_stats_t stats;
    bool enabled=dc_level_view_rigid_solver_stats(view,&stats) && stats.enabled;
    if (!enabled) { dc_level_view_destroy(view,err,sizeof(err)); ASSERT_TRUE(enabled); }
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    for(uint32_t x=40;x<90;++x)
        ASSERT_TRUE(dc_level_view_paint(view,x,80,0,DC_MATERIAL_STONE,err,sizeof(err)));
    dc_gpu_body_shape_t shape={.count=4,.material=DC_GPU_BODY_STONE,
        .vertices={{0,0},{4<<16,0},{4<<16,4<<16},{0,4<<16}}};
    ASSERT_TRUE(dc_level_view_spawn_convex_body(view,63,76,4,4,&shape,err,sizeof(err)));
    shape.material=DC_GPU_BODY_WOOD;
    ASSERT_TRUE(dc_level_view_spawn_convex_body(view,63,72,4,4,&shape,err,sizeof(err)));
    for(uint32_t tick=0;tick<90;++tick) {
        ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    }
    dc_gpu_world_body_t before[2],after;
    dc_gpu_body_motion_t motions[2],motion;
    for(uint32_t id=1;id<=2;++id) {
        ASSERT_TRUE(dc_level_view_body_state(view,id,&before[id-1],&motions[id-1],err,sizeof(err)));
        double y=(double)before[id-1].chunk.y*64+before[id-1].body.y_fp/65536.0;
        printf("stack body %u: y=%.6f vy=%.6f angle=%.6f\n",id,y,
               before[id-1].body.vy_fp/65536.0,motions[id-1].angle);
        ASSERT_TRUE(fabs(y-(INITIAL_CHUNK_Y*64+80-4*(int32_t)id))<.1);
        ASSERT_TRUE(abs(before[id-1].body.vy_fp)<6554);
        ASSERT_TRUE(fabsf(motions[id-1].angle)<.05f);
        ASSERT_TRUE(fabsf(motions[id-1].density-(id==1 ? 2.7f : .6f))<.0001f);
    }
    ASSERT_TRUE(dc_level_view_rigid_solver_stats(view,&stats));
    ASSERT_EQ(stats.active_bodies,2u);ASSERT_EQ(stats.overflow,0u);
    uint32_t color;
    ASSERT_TRUE(dc_level_view_pixel(view,64,74,&color,err,sizeof(err)));ASSERT_EQ(color,0xff30c040u);
    ASSERT_TRUE(dc_level_view_pixel(view,64,78,&color,err,sizeof(err)));ASSERT_EQ(color,0xff30c040u);
    ASSERT_TRUE(dc_level_view_pan_pixels(view,16*DC_CHUNK_SIDE,0));
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view,-16*(int32_t)DC_CHUNK_SIDE,0));
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
    view=dc_level_view_create(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view,false));
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    for(uint32_t id=1;id<=2;++id) {
        ASSERT_TRUE(dc_level_view_body_state(view,id,&after,&motion,err,sizeof(err)));
        ASSERT_EQ(memcmp(&before[id-1],&after,sizeof(after)),0);
        ASSERT_EQ(memcmp(&motions[id-1],&motion,sizeof(motion)),0);
    }
    ASSERT_TRUE(dc_level_view_pixel(view,64,74,&color,err,sizeof(err)));
    ASSERT_EQ(color,0xff30c040u);
    for(uint32_t x=40;x<90;++x)
        ASSERT_TRUE(dc_level_view_paint(view,x,80,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_body_state(view,1,&after,&motion,err,sizeof(err)));
    double released_y=(double)after.chunk.y*64+after.body.y_fp/65536.0;
    ASSERT_TRUE(released_y>INITIAL_CHUNK_Y*64+76+.2);
    ASSERT_TRUE(after.body.vy_fp>13107);
    for(uint32_t tick=0;tick<4;++tick) {
        ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pixel(view,64,82,&color,err,sizeof(err)));
    ASSERT_EQ(color,0xff30c040u);
    ASSERT_TRUE(dc_level_view_pixel(view,64,74,&color,err,sizeof(err)));
    ASSERT_TRUE(color!=0xff30c040u);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
    PASS();
}
int main(int argc, char **argv) {
    RUN(test_quarter_native_gpu_stack_settles_and_persists);
    if (argc==2 && strcmp(argv[1],"--solver-only")==0) {
        printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail ? 1 : 0;
    }
    RUN(test_world_contacts_survive_camera_eviction_and_session_reload);
    RUN(test_convex_window_shapes_survive_eviction_and_reload);
    RUN(test_quarter_native_window_keeps_both_spawned_boxes_visible);
    RUN(test_world_boxes_survive_camera_eviction_and_session_reload);
    RUN(test_streamed_window_broadphase_preserves_dense_pair_ids);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
