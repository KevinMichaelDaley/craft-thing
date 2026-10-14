#include "ferrum/renderer/shading.h"
#include <math.h>
#include <stdio.h>

static int g_pass, g_fail;
#define RUN(fn)                                                                \
  do {                                                                         \
    printf("RUN %s\n", #fn);                                                   \
    fn();                                                                      \
  } while (0)
#define ASSERT_TRUE(e)                                                         \
  do {                                                                         \
    if (!(e)) {                                                                \
      printf("FAIL %d: %s\n", __LINE__, #e);                                   \
      g_fail++;                                                                \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++
static void test_opposing_light_energy_survives_direction_cancellation(void) {
  fr_appearance_texel_t a = {
      .color = {.8f, .4f, .2f}, .normal = {1, 0, 0}, .roughness = .6f};
  fr_renderer_light_field_t f = {.l0 = {2, 2, 2, 0}};
  float ambient[3] = {0}, hdr[3];
  fr_renderer_shade(a, f, ambient, hdr);
  ASSERT_TRUE(fabsf(hdr[0] - .8f) < .00001f);
  ASSERT_TRUE(hdr[2] > .1f);
  PASS();
}
static void test_normal_rotates_colored_direction_response(void) {
  fr_appearance_texel_t a = {.color = {1, 1, 1}, .normal = {1, 0, 0}};
  fr_renderer_light_field_t f = {.l0 = {1, 0, 1}, .lx = {1, 0, -1}};
  float ambient[3] = {0}, first[3], second[3];
  fr_renderer_shade(a, f, ambient, first);
  a = fr_renderer_rotate_normal(a, 3.14159265359f);
  fr_renderer_shade(a, f, ambient, second);
  ASSERT_TRUE(first[0] > .9f && first[2] < .01f);
  ASSERT_TRUE(second[2] > .9f && second[0] < .01f);
  PASS();
}
static void test_hdr_display_encoding_once(void) {
  float hdr[3] = {1, 0, 3};
  uint32_t packed = fr_renderer_pack_display(hdr, 1);
  ASSERT_TRUE((packed & 255u) > 180 && (packed & 255u) < 190);
  ASSERT_EQ((packed >> 8) & 255u, 0u);
  ASSERT_TRUE(((packed >> 16) & 255u) > 220);
  ASSERT_EQ(fr_renderer_pack_display(hdr, 0), 0xff000000u);
  PASS();
}
int main(void) {
  RUN(test_opposing_light_energy_survives_direction_cancellation);
  RUN(test_normal_rotates_colored_direction_response);
  RUN(test_hdr_display_encoding_once);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
