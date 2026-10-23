#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/renderer/lighting_gpu.h"

static int g_pass, g_fail;
static dc_gpu_t *g_gpu;
static fr_occlusion_gpu_t *g_occlusion;
static fr_lighting_gpu_t *g_lighting;
static char g_error[256];
enum {
  LIGHT_WIDTH = 17,
  LIGHT_HEIGHT = 9,
  LIGHT_CELLS = LIGHT_WIDTH * LIGHT_HEIGHT
};
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
  fr_renderer_sun_t sun = {.direction = {0, -1, 0},
                           .color = {1, 1, 1},
                           .intensity = 1,
                           .known_sky = FR_SKY_TOP};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting, NULL, 0, sun, 128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t shallow, deep;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 8, 0, &shallow));
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 8, 7, &deep));
  ASSERT_TRUE(deep.l0[0] > 0 && deep.l0[0] < shallow.l0[0] * .98f);
  PASS();
}
static void test_emission_propagates_around_an_open_doorway(void) {
  ASSERT_TRUE(empty_scene());
  for (unsigned y = 0; y < LIGHT_HEIGHT; y++)
    g_opacity[y * LIGHT_WIDTH + 7] = 1;
  g_opacity[4 * LIGHT_WIDTH + 7] = 0;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion, g_opacity, g_known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(g_occlusion, g_error, sizeof(g_error)));
  fr_renderer_point_light_t light = {4.5f, 4.5f, 0, 20, {1, 0, 0}, 10};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting, &light, 1,
                                          (fr_renderer_sun_t){0}, 128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t field;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 9, 6, &field));
  ASSERT_TRUE(field.l0[0] > 0);
  PASS();
}
static void test_large_field_crosses_scan_tiles_and_filters_foliage(void) {
  const unsigned width = 257, height = 129, count = width * height;
  float *opacity = calloc(count, sizeof(*opacity));
  uint32_t *known = malloc(count * sizeof(*known));
  ASSERT_TRUE(opacity && known);
  for (unsigned i = 0; i < count; i++)
    known[i] = 1;
  fr_occlusion_gpu_t *occlusion = NULL;
  fr_lighting_gpu_t *lighting = NULL;
  ASSERT_TRUE(fr_occlusion_gpu_create(g_gpu, width, height, &occlusion, g_error,
                                      sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_create(g_gpu, occlusion, &lighting, g_error,
                                     sizeof(g_error)));
  fr_renderer_sun_t sun = {.direction = {0, -1, 0},
                           .color = {1, 1, 1},
                           .intensity = 1,
                           .known_sky = FR_SKY_TOP};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(lighting, NULL, 0, sun, 1024));
  ASSERT_TRUE(fr_occlusion_gpu_upload(occlusion, opacity, known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(occlusion, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t a, b, c;
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 128, 126, &a));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 128, 127, &b));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 128, 128, &c));
  ASSERT_TRUE(a.l0[0] > b.l0[0] && b.l0[0] > c.l0[0] && c.l0[0] > 0);
  ASSERT_TRUE(c.l0[0] > .9f * b.l0[0]);
  float clear = c.l0[0];
  ASSERT_TRUE(clear > .35f);
  fr_renderer_light_field_t near_canopy, far_canopy, open_sky, near_open;
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 128, 110, &open_sky));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 128, 41, &near_open));
  for (unsigned y = 32; y < 40; y++)
    for (unsigned x = 80; x < 177; x++)
      opacity[y * width + x] = .08f;
  ASSERT_TRUE(fr_occlusion_gpu_upload(occlusion, opacity, known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(occlusion, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(lighting, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 128, 41, &near_canopy));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 128, 110, &far_canopy));
  printf("canopy near %.4f/%.4f far %.4f/%.4f\n", near_canopy.l0[0],
         near_open.l0[0], far_canopy.l0[0], open_sky.l0[0]);
  ASSERT_TRUE(far_canopy.l0[0] > .65f * open_sky.l0[0]);
  ASSERT_TRUE(far_canopy.l0[0] <= open_sky.l0[0]);
  ASSERT_TRUE(far_canopy.l0[0] / open_sky.l0[0] >
              near_canopy.l0[0] / near_open.l0[0] + .1f);
  for (unsigned i = 0; i < count; i++)
    opacity[i] = 0;
  for (unsigned x = 0; x < width; x++)
    opacity[64 * width + x] = .08f;
  ASSERT_TRUE(fr_occlusion_gpu_upload(occlusion, opacity, known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(occlusion, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(lighting, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 128, 128, &c));
  ASSERT_TRUE(c.l0[0] > 0 && c.l0[0] < clear);
  for (unsigned x = 0; x < width; x++)
    opacity[64 * width + x] = 1.f;
  ASSERT_TRUE(fr_occlusion_gpu_upload(occlusion, opacity, known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(occlusion, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(lighting, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 128, 128, &c));
  ASSERT_EQ(c.l0[0], 0.f);
  fr_lighting_gpu_destroy(lighting);
  fr_occlusion_gpu_destroy(occlusion);
  free(opacity);
  free(known);
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
static void test_transport_carries_prefixes_across_recursive_scan_levels(void) {
  const unsigned width = 1093, height = 3, count = width * height;
  float *opacity = calloc(count, sizeof(*opacity));
  uint32_t *known = malloc(count * sizeof(*known));
  ASSERT_TRUE(opacity && known);
  for (unsigned i = 0; i < count; i++)
    known[i] = 1;
  fr_occlusion_gpu_t *occlusion = NULL;
  fr_lighting_gpu_t *lighting = NULL;
  ASSERT_TRUE(fr_occlusion_gpu_create(g_gpu, width, height, &occlusion, g_error,
                                      sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_create(g_gpu, occlusion, &lighting, g_error,
                                     sizeof(g_error)));
  fr_renderer_sun_t sun = {.direction = {-1, 0, 0},
                           .color = {1, 1, 1},
                           .intensity = 1,
                           .known_sky = FR_SKY_LEFT};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(lighting, NULL, 0, sun, 4096));
  ASSERT_TRUE(fr_occlusion_gpu_upload(occlusion, opacity, known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(occlusion, g_error, sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_generate(lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t before, boundary, after;
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 1022, 1, &before));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 1023, 1, &boundary));
  ASSERT_TRUE(fr_lighting_gpu_read(lighting, 1024, 1, &after));
  ASSERT_TRUE(before.l0[0] > boundary.l0[0] && boundary.l0[0] > after.l0[0]);
  ASSERT_TRUE(after.l0[0] > .95f * boundary.l0[0]);
  fr_lighting_gpu_destroy(lighting);
  fr_occlusion_gpu_destroy(occlusion);
  free(opacity);
  free(known);
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
static void test_common_direction_compacts_beams_and_mixed_heights_fall_back(void) {
  ASSERT_TRUE(empty_scene());
  fr_renderer_sun_t sun={.direction={0,-1,1},.color={1,.7f,.4f},.intensity=1,.known_sky=FR_SKY_TOP};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,NULL,0,sun,128));
  ASSERT_EQ(fr_lighting_gpu_beam_stride(g_lighting),16u);
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  fr_renderer_light_field_t before[LIGHT_CELLS],after;
  for(unsigned y=0;y<LIGHT_HEIGHT;y++) for(unsigned x=0;x<LIGHT_WIDTH;x++)
    ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,x,y,&before[y*LIGHT_WIDTH+x]));
  fr_renderer_point_light_t points[2]={{4.5f,4.5f,1,20,{1,0,0},100},{12.5f,4.5f,3,20,{0,0,1},100}};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,points,2,sun,128));
  ASSERT_EQ(fr_lighting_gpu_beam_stride(g_lighting),20u);
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,NULL,0,sun,128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  for(unsigned y=0;y<LIGHT_HEIGHT;y++) for(unsigned x=0;x<LIGHT_WIDTH;x++) {
    ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,x,y,&after));
    ASSERT_EQ(memcmp(&after,&before[y*LIGHT_WIDTH+x],sizeof(after)),0);
  }
  PASS();
}
static void test_common_direction_matches_general_transport(void) {
  ASSERT_TRUE(empty_scene());
  for(unsigned y=0;y<LIGHT_HEIGHT;y++) g_opacity[y*LIGHT_WIDTH+7]=y==4?0.f:1.f;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion,g_opacity,g_known));
  fr_lighting_gpu_t *reference=NULL;
  ASSERT_EQ(setenv("DC_LIGHTING_GENERAL_TRANSPORT","1",1),0);
  bool created=fr_lighting_gpu_create(g_gpu,g_occlusion,&reference,g_error,sizeof(g_error));
  ASSERT_EQ(unsetenv("DC_LIGHTING_GENERAL_TRANSPORT"),0);ASSERT_TRUE(created);
  fr_renderer_point_light_t points[2]={{4.5f,4.5f,2,20,{1,.2f,.3f},100},{12.5f,4.5f,2,20,{.1f,.3f,1},100}};
  fr_renderer_sun_t sky={.direction={-.3f,-.8f,.5f},.color={1,.7f,.4f},.intensity=1.4f,.known_sky=FR_SKY_TOP|FR_SKY_LEFT};
  for(unsigned point_case=0;point_case<3;point_case++) {
    const fr_renderer_point_light_t *p=point_case?points:NULL;
    uint32_t count=point_case?2u:0u;
    fr_renderer_sun_t sun=point_case==1?(fr_renderer_sun_t){0}:sky;
    ASSERT_TRUE(fr_lighting_gpu_set_sources(reference,p,count,sun,128));
    ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,p,count,sun,128));
    ASSERT_EQ(fr_lighting_gpu_scan_channels(g_lighting),point_case==2?7u:4u);
    ASSERT_TRUE(fr_lighting_gpu_generate(reference,g_error,sizeof(g_error)));
    ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
    for(unsigned y=0;y<LIGHT_HEIGHT;y++) for(unsigned x=0;x<LIGHT_WIDTH;x++) {
      fr_renderer_light_field_t a,b;
      ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,x,y,&a));
      ASSERT_TRUE(fr_lighting_gpu_read(reference,x,y,&b));
      for(unsigned c=0;c<3;c++) {
        float scale=fmaxf(.01f,b.l0[c]);
        ASSERT_TRUE(fabsf(a.l0[c]-b.l0[c])<.0001f*scale);
        ASSERT_TRUE(fabsf(a.lx[c]-b.lx[c])<.006f*scale);
        ASSERT_TRUE(fabsf(a.ly[c]-b.ly[c])<.006f*scale);
        ASSERT_TRUE(fabsf(a.lz[c]-b.lz[c])<.006f*scale);
      }
    }
  }
  fr_lighting_gpu_destroy(reference);PASS();
}
static void test_mixed_heights_preserve_independent_vertical_moments(void) {
  ASSERT_TRUE(empty_scene());
  fr_renderer_point_light_t points[2]={{8.5f,4.5f,1,20,{1,0,0},100},{8.5f,4.5f,3,20,{0,0,1},100}};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,points,2,(fr_renderer_sun_t){0},128));
  ASSERT_EQ(fr_lighting_gpu_scan_channels(g_lighting),7u);
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  fr_renderer_light_field_t f;ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,8,4,&f));
  ASSERT_TRUE(f.l0[0]>0&&f.l0[2]>0);
  ASSERT_TRUE(fabsf(f.lz[0]/f.l0[0]-sqrtf(.5f))<.004f);
  ASSERT_TRUE(fabsf(f.lz[2]/f.l0[2]-3.f/sqrtf(10.f))<.004f);
  PASS();
}
static void test_light_edits_read_authoritative_optics_without_tree_rebuild(void) {
  ASSERT_TRUE(empty_scene());
  fr_renderer_point_light_t point={4.5f,4.5f,0,20,{1,0,0},10};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,&point,1,(fr_renderer_sun_t){0},128));
  for(unsigned y=0;y<LIGHT_HEIGHT;y++) g_opacity[y*LIGHT_WIDTH+7]=1.f;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion,g_opacity,g_known));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  fr_renderer_light_field_t field;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,12,4,&field));
  ASSERT_EQ(field.l0[0],0.f);
  g_opacity[4*LIGHT_WIDTH+7]=0.f;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion,g_opacity,g_known));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,12,4,&field));
  ASSERT_TRUE(field.l0[0]>0.f);
  PASS();
}
static void test_diagonal_schedule_excludes_absent_tiles(void) {
  const uint32_t width = 193, height = 65;
  fr_occlusion_gpu_t *occlusion = NULL;
  fr_lighting_gpu_t *lighting = NULL;
  ASSERT_TRUE(fr_occlusion_gpu_create(g_gpu,width,height,&occlusion,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_create(g_gpu,occlusion,&lighting,g_error,sizeof(g_error)));
  uint32_t expected = 0;
  for (uint32_t along = 0; along < height; along += 32u)
    expected += width + height - 1u - 2u * along;
  ASSERT_EQ(fr_lighting_gpu_scan_groups(lighting,1),expected);
  float *opacity = calloc(width * height,sizeof(*opacity));
  uint32_t *known = malloc(width * height * sizeof(*known));
  ASSERT_TRUE(opacity && known);
  for (uint32_t i = 0; i < width * height; i++) known[i] = 1u;
  ASSERT_TRUE(fr_occlusion_gpu_upload(occlusion,opacity,known));
  ASSERT_TRUE(fr_occlusion_gpu_generate(occlusion,g_error,sizeof(g_error)));
  fr_renderer_point_light_t point = {96.5f,32.5f,0,1000,{1,.3f,.7f},100};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(lighting,&point,1,(fr_renderer_sun_t){0},1024));
  ASSERT_TRUE(fr_lighting_gpu_generate(lighting,g_error,sizeof(g_error)));
  for (uint32_t y = 0; y < height; y++) {
    fr_renderer_light_field_t a,b;
    ASSERT_TRUE(fr_lighting_gpu_read(lighting,1,y,&a));
    ASSERT_TRUE(fr_lighting_gpu_read(lighting,width-2u,height-1u-y,&b));
    for (uint32_t color = 0; color < 3; color++)
      ASSERT_TRUE(fabsf(a.l0[color]-b.l0[color])<.00001f);
  }
  free(opacity);free(known);
  fr_lighting_gpu_destroy(lighting);
  fr_occlusion_gpu_destroy(occlusion);
  PASS();
}
static void test_hdr_emission_preserves_energy_range_and_vertical_direction(void) {
  ASSERT_TRUE(empty_scene());
  fr_renderer_point_light_t light = {4.5f, 4.5f, 1, 20, {1, .5f, .25f}, 1e8f};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting, &light, 1,
                                          (fr_renderer_sun_t){0}, 128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting, g_error, sizeof(g_error)));
  fr_renderer_light_field_t field;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 8, 4, &field));
  ASSERT_TRUE(isfinite(field.l0[0]) && field.l0[0] > 1e6f);
  ASSERT_TRUE(fabsf(field.l0[0] - 2.f * field.l0[1]) < field.l0[0] * 1e-5f);
  ASSERT_TRUE(fabsf(field.lz[0] / field.l0[0] - sqrtf(.5f)) < .003f);
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
  float direct = f.l0[0];
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting, 5, 5, &f));
  ASSERT_TRUE(f.l0[0] > 0 && f.l0[0] < direct);
  PASS();
}
static void test_unknown_boundaries_and_propagation_reach_cannot_leak(void) {
  ASSERT_TRUE(empty_scene());
  ASSERT_TRUE(set_points());
  for (unsigned y = 0; y < LIGHT_HEIGHT; y++)
    g_known[y * LIGHT_WIDTH + 7] = 0;
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
static void test_source_buffer_injection_is_bounded_and_combines_emitters(void) {
  ASSERT_TRUE(empty_scene());
  fr_renderer_point_light_t points[2] = {
      {4.5f,4.5f,1,1,{1,0,0},12},
      {4.5f,4.5f,1,1,{0,0,1},20}};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,points,2,(fr_renderer_sun_t){0},128));
  ASSERT_TRUE(fr_lighting_gpu_source_cells(g_lighting)<=128u);
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  fr_renderer_light_field_t combined, separate;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,4,4,&separate));
  points[0].color[2]=20.f/12.f;
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,points,1,(fr_renderer_sun_t){0},128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,4,4,&combined));
  for(unsigned c=0;c<3;c++) {
    ASSERT_TRUE(fabsf(combined.l0[c]-separate.l0[c])<1e-4f);
    ASSERT_TRUE(fabsf(combined.lz[c]-separate.lz[c])<1e-4f);
  }
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,NULL,0,(fr_renderer_sun_t){0},128));
  ASSERT_EQ(fr_lighting_gpu_source_cells(g_lighting),0u);
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,4,4,&combined));
  ASSERT_EQ(combined.l0[0],0.f);
  ASSERT_EQ(combined.l0[2],0.f);
  PASS();
}
static void test_source_buffer_survives_optical_edits_without_reinjection(void) {
  ASSERT_TRUE(empty_scene());
  fr_renderer_point_light_t point={4.5f,4.5f,1,20,{1,0,0},12};
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,&point,1,(fr_renderer_sun_t){0},128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  uint32_t builds=fr_lighting_gpu_source_builds(g_lighting);
  for(unsigned y=0;y<LIGHT_HEIGHT;y++) g_opacity[y*LIGHT_WIDTH+7]=1.f;
  ASSERT_TRUE(fr_occlusion_gpu_upload(g_occlusion,g_opacity,g_known));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  ASSERT_EQ(fr_lighting_gpu_source_builds(g_lighting),builds);
  fr_renderer_light_field_t field;
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,9,4,&field));
  ASSERT_EQ(field.l0[0],0.f);
  point.color[0]=0;point.color[2]=1;
  ASSERT_TRUE(fr_lighting_gpu_set_sources(g_lighting,&point,1,(fr_renderer_sun_t){0},128));
  ASSERT_TRUE(fr_lighting_gpu_generate(g_lighting,g_error,sizeof(g_error)));
  ASSERT_EQ(fr_lighting_gpu_source_builds(g_lighting),builds+1u);
  ASSERT_TRUE(fr_lighting_gpu_read(g_lighting,4,4,&field));
  ASSERT_EQ(field.l0[0],0.f);
  ASSERT_TRUE(field.l0[2]>0.f);
  PASS();
}
int main(void) {
  if (!dc_gpu_create(&g_gpu, 64, 64, "build/shaders/pattern.comp.spv", g_error,
                     sizeof(g_error)) ||
      !fr_occlusion_gpu_create(g_gpu, LIGHT_WIDTH, LIGHT_HEIGHT, &g_occlusion,
                               g_error, sizeof(g_error)) ||
      !fr_lighting_gpu_create(g_gpu, g_occlusion, &g_lighting, g_error,
                              sizeof(g_error))) {
    fprintf(stderr, "Fixture failure: %s\n", g_error);
    return 1;
  }
  RUN(test_colored_directions_keep_independent_rgb_moments);
  RUN(test_skylight_loses_energy_with_depth);
  RUN(test_emission_propagates_around_an_open_doorway);
  RUN(test_large_field_crosses_scan_tiles_and_filters_foliage);
  RUN(test_transport_carries_prefixes_across_recursive_scan_levels);
  RUN(test_zero_distance_emitter_is_finite);
  RUN(test_common_direction_compacts_beams_and_mixed_heights_fall_back);
  RUN(test_common_direction_matches_general_transport);
  RUN(test_mixed_heights_preserve_independent_vertical_moments);
  RUN(test_source_buffer_injection_is_bounded_and_combines_emitters);
  RUN(test_source_buffer_survives_optical_edits_without_reinjection);
  RUN(test_light_edits_read_authoritative_optics_without_tree_rebuild);
  RUN(test_diagonal_schedule_excludes_absent_tiles);
  RUN(test_hdr_emission_preserves_energy_range_and_vertical_direction);
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
