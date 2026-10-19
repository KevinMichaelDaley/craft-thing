#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ferrum/renderer/lighting_gpu.h"

static int g_pass, g_fail;
static dc_gpu_t *g_gpu;
static fr_occlusion_gpu_t *g_occlusion;
static fr_lighting_gpu_t *g_lighting;
static char g_error[256];
enum { LIGHT_WIDTH=17,LIGHT_HEIGHT=9,LIGHT_CELLS=LIGHT_WIDTH*LIGHT_HEIGHT };
static float g_opacity[LIGHT_CELLS];
static uint32_t g_known[LIGHT_CELLS];
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
  for (unsigned i = 0; i < LIGHT_CELLS; i++) {
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
  for(unsigned y=0;y<LIGHT_HEIGHT;y++) g_opacity[y*LIGHT_WIDTH+7]=1;
  g_opacity[4*LIGHT_WIDTH+7]=0;
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
static void test_large_field_crosses_scan_tiles_and_filters_foliage(void) {
  const unsigned width=257,height=129,count=width*height;
  float *opacity=calloc(count,sizeof(*opacity));uint32_t *known=malloc(count*sizeof(*known));
  ASSERT_TRUE(opacity&&known);
  for(unsigned i=0;i<count;i++) known[i]=1;
  fr_occlusion_gpu_t *occlusion=NULL;fr_lighting_gpu_t *lighting=NULL;
  ASSERT_TRUE(fr_occlusion_gpu_create(g_gpu,width,height,&occlusion,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_create(g_gpu,occlusion,&lighting,g_error,sizeof(g_error)));
  fr_renderer_sun_t sun={.direction={0,-1,0},.color={1,1,1},.intensity=1,.known_sky=FR_SKY_TOP};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(lighting,NULL,0,sun,1024));
  ASSERT_TRUE(fr_occlusion_gpu_upload(occlusion,opacity,known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(occlusion,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(lighting,g_error,sizeof(g_error)));
  fr_renderer_light_field_t a,b,c;
  ASSERT_TRUE(fr_lighting_gpu_read(lighting,128,126,&a));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting,128,127,&b));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting,128,128,&c));
  ASSERT_TRUE(a.l0[0]>b.l0[0]&&b.l0[0]>c.l0[0]&&c.l0[0]>0);
  ASSERT_TRUE(c.l0[0]>.9f*b.l0[0]);float clear=c.l0[0];
  ASSERT_TRUE(clear>.35f);
  for(unsigned y=32;y<40;y++)
    for(unsigned x=80;x<177;x++) opacity[y*width+x]=.08f;
  ASSERT_TRUE(fr_occlusion_gpu_upload(occlusion,opacity,known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(occlusion,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(lighting,g_error,sizeof(g_error)));
  fr_renderer_light_field_t near_canopy,far_canopy,open_sky;
  ASSERT_TRUE(fr_lighting_gpu_read(lighting,128,41,&near_canopy));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting,128,110,&far_canopy));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting,32,110,&open_sky));
  printf("canopy near %.4f far %.4f open %.4f\n",near_canopy.l0[0],far_canopy.l0[0],open_sky.l0[0]);
  ASSERT_TRUE(far_canopy.l0[0]>.65f*open_sky.l0[0]);
  ASSERT_TRUE(far_canopy.l0[0]/open_sky.l0[0]>near_canopy.l0[0]);
  for(unsigned i=0;i<count;i++) opacity[i]=0;
  for(unsigned x=0;x<width;x++) opacity[64*width+x]=.08f;
  ASSERT_TRUE(fr_occlusion_gpu_upload(occlusion,opacity,known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(occlusion,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(lighting,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting,128,128,&c));
  ASSERT_TRUE(c.l0[0]>0&&c.l0[0]<clear);
  for(unsigned x=0;x<width;x++) opacity[64*width+x]=1.f;
  ASSERT_TRUE(fr_occlusion_gpu_upload(occlusion,opacity,known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(occlusion,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(lighting,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting,128,128,&c));ASSERT_EQ(c.l0[0],0.f);
  fr_lighting_gpu_destroy(lighting);fr_occlusion_gpu_destroy(occlusion);free(opacity);free(known);PASS();
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
  for (unsigned y = 0; y < LIGHT_HEIGHT; y++)
    g_opacity[y * LIGHT_WIDTH + 7] = 1;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion, g_opacity, g_known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(g_occlusion, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t f;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 9, 4, &f));
  ASSERT_EQ(f.l0[0], 0.f);
  ASSERT_TRUE(f.l0[2] > 0);
  g_opacity[4 * LIGHT_WIDTH + 7] = 0;
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
  for (unsigned x = 0; x < LIGHT_WIDTH; x++)
    g_opacity[3 * LIGHT_WIDTH + x] = 1;
  g_opacity[3 * LIGHT_WIDTH + 4] = 0;
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
  float direct=f.l0[0];
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 5, 5, &f));
  ASSERT_TRUE(f.l0[0]>0&&f.l0[0]<direct);
  PASS();
}
static void test_unknown_boundaries_and_propagation_reach_cannot_leak(void) {
  ASSERT_TRUE(empty_scene());
  ASSERT_TRUE(set_points());
  for(unsigned y=0;y<LIGHT_HEIGHT;y++) g_known[y*LIGHT_WIDTH+7]=0;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion, g_opacity, g_known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(g_occlusion, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t f;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 9, 4, &f));
  ASSERT_EQ(f.l0[0], 0.f);
  ASSERT_TRUE(empty_scene());
  for (unsigned y = 0; y < LIGHT_HEIGHT; y++)
    g_opacity[y * LIGHT_WIDTH + 7] = 1;
  g_opacity[4 * LIGHT_WIDTH + 7] = 0;
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
      !fr_occlusion_gpu_create(g_gpu, LIGHT_WIDTH, LIGHT_HEIGHT, &g_occlusion, g_error,
                               sizeof(g_error)) ||
      !fr_lighting_gpu_create(g_gpu, g_occlusion, &g_lighting, g_error,
                              sizeof(g_error))) {
    fprintf(stderr, "Fixture failure: %s\n", g_error);
    return 1;
  }
  RUN(test_colored_directions_keep_independent_rgb_moments);
  RUN(test_skylight_loses_energy_with_depth);
  RUN(test_emission_propagates_around_an_open_doorway);
  RUN(test_large_field_crosses_scan_tiles_and_filters_foliage);
  RUN(test_zero_distance_emitter_is_finite);
  RUN(test_thin_wall_and_one_pixel_hole_visibility);
  RUN(test_sun_requires_explicit_known_sky_boundary);
  RUN(test_opposing_equal_lights_keep_energy_when_direction_cancels);
  RUN(test_sun_thin_wall_preserves_one_pixel_gap);
  RUN(test_unknown_boundaries_and_propagation_reach_cannot_leak);
  fr_lighting_gpu_destroy(g_lighting);
  fr_occlusion_gpu_destroy(g_occlusion);
  dc_gpu_destroy(g_gpu);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
