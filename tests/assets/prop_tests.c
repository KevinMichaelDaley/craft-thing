#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/assets/prop.h"

static int g_pass, g_fail;
static uint8_t *g_bytes;
static size_t g_size;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++g_fail; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() ++g_pass

static void test_namespaces(void) {
    ASSERT_INT_EQ(fr_prop_terrain_material(2), 7);
    ASSERT_INT_EQ(fr_prop_body_material(2), 2);
    ASSERT_INT_EQ(fr_prop_body_material(5), 0);
    ASSERT_INT_EQ(fr_prop_terrain_material(3), 0);
    PASS();
}
static void test_real_compiler_asset_holes_and_appearance(void) {
    fr_prop_asset_t asset = {0}; char error[128];
    ASSERT_TRUE(fr_prop_asset_open(&asset, g_bytes, g_size, error, sizeof(error)));
    ASSERT_INT_EQ(asset.width, 3); ASSERT_INT_EQ(asset.height, 2);
    fr_prop_pixel_t solid, hole;
    ASSERT_TRUE(fr_prop_pixel(&asset, 0, 0, &solid));
    ASSERT_TRUE(fr_prop_pixel(&asset, 0, 1, &hole));
    ASSERT_INT_EQ(solid.material, 2); ASSERT_INT_EQ(solid.role, 3);
    ASSERT_INT_EQ(hole.material, 0);
    fr_prop_appearance_t app;
    ASSERT_TRUE(fr_prop_appearance(&asset, 0, 0, &app));
    ASSERT_TRUE(app.color[0] > .30f && app.color[0] < .31f);
    ASSERT_EQ(app.emission[0], 1.f);
    ASSERT_TRUE(!fr_prop_pixel(&asset, 3, 0, &hole));
    PASS();
}
static void test_elapsed_frame_timing(void) {
    fr_prop_asset_t asset = {0};
    ASSERT_TRUE(fr_prop_asset_open(&asset, g_bytes, g_size, NULL, 0));
    ASSERT_INT_EQ(fr_prop_animation_frame(&asset, 89), 0);
    ASSERT_INT_EQ(fr_prop_animation_frame(&asset, 90), 1);
    ASSERT_INT_EQ(fr_prop_animation_frame(&asset, 259), 1);
    ASSERT_INT_EQ(fr_prop_animation_frame(&asset, 260), 0);
    PASS();
}
static void test_failure_keeps_previous_view(void) {
    fr_prop_asset_t asset = {0}, before;
    ASSERT_TRUE(fr_prop_asset_open(&asset, g_bytes, g_size, NULL, 0));
    before = asset;
    g_bytes[g_size - 1] ^= 1;
    ASSERT_TRUE(!fr_prop_asset_open(&asset, g_bytes, g_size, NULL, 0));
    g_bytes[g_size - 1] ^= 1;
    ASSERT_TRUE(memcmp(&asset, &before, sizeof(asset)) == 0);
    ASSERT_TRUE(!fr_prop_asset_open(&asset, g_bytes, 63, NULL, 0));
    PASS();
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *file = fopen(argv[1], "rb"); if (!file) return 2;
    fseek(file, 0, SEEK_END); long length = ftell(file); rewind(file);
    if (length <= 0) { fclose(file); return 2; }
    g_size = (size_t)length; g_bytes = malloc(g_size);
    if (!g_bytes || fread(g_bytes, 1, g_size, file) != g_size) { fclose(file); free(g_bytes); return 2; }
    fclose(file);
    RUN(test_namespaces); RUN(test_real_compiler_asset_holes_and_appearance);
    RUN(test_elapsed_frame_timing); RUN(test_failure_keeps_previous_view);
    free(g_bytes); printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
