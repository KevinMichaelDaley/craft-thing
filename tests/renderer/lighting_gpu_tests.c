#include <math.h>
#include <stdio.h>

#include "ferrum/renderer/lighting_gpu.h"

static int g_pass, g_fail;
static dc_gpu_t *g_gpu;
static fr_occlusion_gpu_t *g_occlusion;
static fr_lighting_gpu_t *g_lighting;
static char g_error[256];
static float g_opacity[128];
static uint32_t g_known[128];
#define RUN(fn)                                                                \
  do {                                                                         \
    printf("RUN %s\n", #fn);                                                   \
    fn();                                                                      \
  } while (0)
#define ASSERT_TRUE(e)                                                         \
  do {                                                                         \
    if (!(e)) {                                                                \
      printf("FAIL %d: %s (%s)\n", __LINE__, #e, g_error);                     \
      g_fail++;                                                                \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static bool empty_scene(void) {
  for (unsigned i = 0; i < 128; i++) {
    g_opacity[i] = 0;
    g_known[i] = 1;
  }
  return fr_occlusion_gpu_upload(g_occlusion, g_opacity, g_known) &&
         fr_occlusion_gpu_generate(g_occlusion, g_error, sizeof(g_error));
}
static bool set_points(void) {
  fr_renderer_point_light_t points[2] = {{4.5f, 4.5f, 0, 20, {1, 0, 0}, 10},
                                         {12.5f, 4.5f, 0, 20, {0, 0, 1}, 10}};
  fr_renderer_sun_t sun = {.direction = {0, -1, 0}, .color = {1, 1, 1}};
  return fr_lighting_gpu_set_sources(g_lighting, points, 2, sun, 128) &&
         fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error));
}
static void test_skylight_loses_energy_with_depth(void) {
  ASSERT_TRUE(empty_scene());
  fr_renderer_sun_t sun={.direction={0,-1,0},.color={1,1,1},.intensity=1,.known_sky=FR_SKY_TOP};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,NULL,0,sun,128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  fr_renderer_light_field_t shallow,deep;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,8,0,&shallow));
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,8,7,&deep));
  ASSERT_TRUE(deep.l0[0]>0&&deep.l0[0]<shallow.l0[0]*.98f);
  PASS();
}
static void test_emission_propagates_around_an_open_doorway(void) {
  ASSERT_TRUE(empty_scene());
  for(unsigned y=0;y<8;y++) g_opacity[y*16+7]=1;
  g_opacity[4*16+7]=0;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion,g_opacity,g_known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(g_occlusion,g_error,sizeof(g_error)));
  fr_renderer_point_light_t light={4.5f,4.5f,0,20,{1,0,0},10};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,&light,1,(fr_renderer_sun_t){0},128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  fr_renderer_light_field_t field;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,9,6,&field));
  ASSERT_TRUE(field.l0[0]>0);
  PASS();
}
static void test_colored_directions_keep_independent_rgb_moments(void) {
  ASSERT_TRUE(empty_scene());
  ASSERT_TRUE(set_points());
  fr_renderer_light_field_t f;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 8, 4, &f));
  ASSERT_TRUE(f.l0[0] > 0 && f.l0[2] > 0);
  ASSERT_TRUE(f.lx[0] < 0 && f.lx[2] > 0);
  PASS();
}
static void test_zero_distance_emitter_is_finite(void) {
  ASSERT_TRUE(empty_scene());
  ASSERT_TRUE(set_points());
  fr_renderer_light_field_t f;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 4, 4, &f));
  ASSERT_TRUE(isfinite(f.l0[0]) && f.l0[0] > 0);
  PASS();
}
static void test_thin_wall_and_one_pixel_hole_visibility(void) {
  ASSERT_TRUE(empty_scene());
  ASSERT_TRUE(set_points());
  for (unsigned y = 0; y < 8; y++)
    g_opacity[y * 16 + 7] = 1;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion, g_opacity, g_known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(g_occlusion, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t f;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 9, 4, &f));
  ASSERT_EQ(f.l0[0], 0.f);
  ASSERT_TRUE(f.l0[2] > 0);
  g_opacity[4 * 16 + 7] = 0;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion, g_opacity, g_known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(g_occlusion, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 9, 4, &f));
  ASSERT_TRUE(f.l0[0] > 0);
  PASS();
}
static void test_sun_requires_explicit_known_sky_boundary(void) {
  ASSERT_TRUE(empty_scene());
  fr_renderer_sun_t sun = {.direction = {0, -1, 0},
                           .color = {1, 1, 1},
                           .intensity = 1,
                           .known_sky = FR_SKY_TOP};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting, NULL, 0, sun, 128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t f;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 4, 4, &f));
  ASSERT_TRUE(f.l0[0] > 0);
  sun.known_sky = 0;
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting, NULL, 0, sun, 128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 4, 4, &f));
  ASSERT_EQ(f.l0[0], 0.f);
  PASS();
}
static void
test_opposing_equal_lights_keep_energy_when_direction_cancels(void) {
  ASSERT_TRUE(empty_scene());
  fr_renderer_point_light_t points[2] = {{4.5f, 4.5f, 0, 20, {1, 1, 1}, 10},
                                         {12.5f, 4.5f, 0, 20, {1, 1, 1}, 10}};
  fr_renderer_sun_t sun = {0};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting, points, 2, sun, 128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t f;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 8, 4, &f));
  ASSERT_TRUE(f.l0[0] > 0);
  ASSERT_TRUE(fabsf(f.lx[0]) < .00001f);
  PASS();
}
static void test_sun_thin_wall_preserves_one_pixel_gap(void) {
  ASSERT_TRUE(empty_scene());
  for (unsigned x = 0; x < 16; x++)
    g_opacity[3 * 16 + x] = 1;
  g_opacity[3 * 16 + 4] = 0;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion, g_opacity, g_known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(g_occlusion, g_error, sizeof(g_error)));
  fr_renderer_sun_t sun = {.direction = {0, -1, 0},
                           .color = {1, 1, 1},
                           .intensity = 1,
                           .known_sky = FR_SKY_TOP};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting, NULL, 0, sun, 128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t f;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 4, 5, &f));
  ASSERT_TRUE(f.l0[0] > 0);
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 5, 5, &f));
  ASSERT_EQ(f.l0[0], 0.f);
  PASS();
}
static void test_unknown_and_unfinished_rays_cannot_leak(void) {
  ASSERT_TRUE(empty_scene());
  ASSERT_TRUE(set_points());
  g_known[4 * 16 + 7] = 0;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion, g_opacity, g_known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(g_occlusion, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t f;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 9, 4, &f));
  ASSERT_EQ(f.l0[0], 0.f);
  ASSERT_TRUE(empty_scene());
  for (unsigned y = 0; y < 8; y++)
    g_opacity[y * 16 + 7] = 1;
  g_opacity[4 * 16 + 7] = 0;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion, g_opacity, g_known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(g_occlusion, g_error, sizeof(g_error)));
  fr_renderer_point_light_t point = {14.5f, 4.5f, 0, 32, {1, 0, 0}, 10};
  fr_renderer_sun_t sun = {0};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting, &point, 1, sun, 1));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 0, 4, &f));
  ASSERT_EQ(f.l0[0], 0.f);
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting, &point, 1, sun, 128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 0, 4, &f));
  ASSERT_TRUE(f.l0[0] > 0);
  PASS();
}
int main(void) {
  if (!dc_gpu_create(&g_gpu, 64, 64, "build/shaders/pattern.comp.spv", g_error,
                     sizeof(g_error)) ||
      !fr_occlusion_gpu_create(g_gpu, 16, 8, &g_occlusion, g_error,
                               sizeof(g_error)) ||
      !fr_lighting_gpu_create(g_gpu, g_occlusion, &g_lighting, g_error,
                              sizeof(g_error))) {
    fprintf(stderr, "Fixture failure: %s\n", g_error);
    return 1;
  }
  RUN(test_colored_directions_keep_independent_rgb_moments);
  RUN(test_skylight_loses_energy_with_depth);
  RUN(test_emission_propagates_around_an_open_doorway);
  RUN(test_zero_distance_emitter_is_finite);
  RUN(test_thin_wall_and_one_pixel_hole_visibility);
  RUN(test_sun_requires_explicit_known_sky_boundary);
  RUN(test_opposing_equal_lights_keep_energy_when_direction_cancels);
  RUN(test_sun_thin_wall_preserves_one_pixel_gap);
  RUN(test_unknown_and_unfinished_rays_cannot_leak);
  fr_lighting_gpu_destroy(g_lighting);
  fr_occlusion_gpu_destroy(g_occlusion);
  dc_gpu_destroy(g_gpu);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
