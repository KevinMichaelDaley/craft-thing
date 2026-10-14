#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/chunk.h"
#include "ferrum/renderer/appearance.h"

static int g_pass, g_fail;
#define RUN(fn)                                                                \
  do {                                                                         \
    printf("RUN %s\n", #fn);                                                   \
    fn();                                                                      \
  } while (0)
#define ASSERT_TRUE(e)                                                         \
  do {                                                                         \
    if (!(e)) {                                                                \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #e);                      \
      g_fail++;                                                                \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_mixed_material_holes_and_visual_scale(void) {
  uint32_t materials[4] = {DC_MATERIAL_WOOD, 0, DC_MATERIAL_STONE,
                           DC_MATERIAL_DENSE_WOOD};
  uint32_t p = 99, m = 99;
  ASSERT_TRUE(fr_appearance_resolve(2, 2, 4, 3, 2, materials, &p, &m));
  ASSERT_INT_EQ(p, 0);
  ASSERT_INT_EQ(m, DC_MATERIAL_WOOD);
  ASSERT_TRUE(!fr_appearance_resolve(2, 2, 4, 5, 2, materials, &p, &m));
  ASSERT_TRUE(fr_appearance_resolve(2, 2, 4, 7, 7, materials, &p, &m));
  ASSERT_INT_EQ(p, 3);
  ASSERT_INT_EQ(m, DC_MATERIAL_DENSE_WOOD);
  ASSERT_TRUE(!fr_appearance_resolve(2, 2, 4, 8, 0, materials, &p, &m));
  ASSERT_TRUE(!fr_appearance_resolve(2, 2, 0, 0, 0, materials, &p, &m));
  PASS();
}
static void test_slot_generations(void) {
  fr_appearance_owner_t owner = {.identity = 42, .generation = 7};
  ASSERT_TRUE(fr_appearance_matches(&owner, 42, 7));
  ASSERT_TRUE(!fr_appearance_matches(&owner, 42, 8));
  ASSERT_TRUE(!fr_appearance_matches(&owner, 43, 7));
  PASS();
}
static void test_material_namespaces(void) {
  uint32_t material = 99;
  ASSERT_TRUE(fr_appearance_material(FR_APPEARANCE_BODY, 2, &material));
  ASSERT_INT_EQ(material, DC_MATERIAL_WOOD);
  ASSERT_TRUE(fr_appearance_material(FR_APPEARANCE_TERRAIN, 2, &material));
  ASSERT_INT_EQ(material, DC_MATERIAL_SAND);
  ASSERT_TRUE(!fr_appearance_material(FR_APPEARANCE_BODY, 15, &material));
  PASS();
}
static void test_default_and_explicit_color_independence(void) {
  fr_appearance_owner_t o = {.identity = 13,
                             .generation = 2,
                             .x = -65,
                             .y = 4,
                             .material = 7,
                             .visual_scale = 1};
  fr_appearance_texel_t a = fr_appearance_default(o),
                        b = fr_appearance_default(o);
  ASSERT_TRUE(memcmp(&a, &b, sizeof(a)) == 0);
  ASSERT_TRUE(a.color[0] > 0 && a.normal[2] == 1 && a.roughness > 0);
  b.color[0] = .9f;
  b.color[1] = .01f;
  ASSERT_TRUE(memcmp(&a, &b, sizeof(a)) != 0);
  ASSERT_EQ(o.material, 7u);
  PASS();
}
static void test_rest_transform(void) {
  float x, y;
  fr_appearance_rest_point(10, 23, 10, 20, 1.57079632679f, &x, &y);
  ASSERT_TRUE(fabsf(x - 3) < .00001f && fabsf(y) < .00001f);
  PASS();
}
static void test_explicit_sidecar_ownership_and_recycle(void) {
  fr_appearance_texel_t colors[4],
      a = {.color = {.8f, .1f, .05f}, .normal = {0, 0, 1}, .roughness = .7f}, b;
  uint32_t mapping[4], index;
  uint8_t valid[4];
  fr_appearance_sidecar_t s;
  fr_appearance_owner_t owner = {
      .identity = 12, .generation = 3, .visual_scale = 2};
  ASSERT_TRUE(fr_appearance_bind(&s, owner, colors, mapping, valid, 4));
  ASSERT_TRUE(fr_appearance_write(&s, 0, 9, a));
  a.color[0] = .2f;
  ASSERT_TRUE(fr_appearance_write(&s, 1, 9, a));
  ASSERT_TRUE(fr_appearance_read(&s, 0, 12, 3, &index, &b));
  ASSERT_EQ(index, 9u);
  ASSERT_TRUE(b.color[0] == .8f);
  ASSERT_TRUE(fr_appearance_read(&s, 1, 12, 3, &index, &b));
  ASSERT_EQ(index, 9u);
  ASSERT_TRUE(b.color[0] == .2f);
  ASSERT_TRUE(!fr_appearance_read(&s, 0, 12, 4, &index, &b));
  owner.generation = 4;
  ASSERT_TRUE(fr_appearance_bind(&s, owner, colors, mapping, valid, 4));
  ASSERT_TRUE(!fr_appearance_read(&s, 0, 12, 4, &index, &b));
  PASS();
}
static void test_import_material_defaults_and_independent_emission(void) {
  fr_appearance_texel_t defaults = {
      .color = {.1f, .2f, .3f}, .roughness = .85f,
      .normal = {.1f, .2f, .97f}, .relief = .4f};
  fr_appearance_source_t source = {
      .surface = {.color = {.9f, .05f, .2f}, .roughness = .8f,
                  .normal = {0, 0, 1}, .relief = 0},
      .emission = {100, 2, .5f}, .alpha = 0};
  fr_appearance_source_t output;
  ASSERT_TRUE(fr_appearance_import(&source, &defaults, &output));
  ASSERT_EQ(output.surface.color[0], .9f);
  ASSERT_EQ(output.surface.roughness, defaults.roughness);
  ASSERT_TRUE(memcmp(output.surface.normal, defaults.normal,
                     sizeof(defaults.normal)) == 0);
  ASSERT_EQ(output.surface.relief, defaults.relief);
  ASSERT_TRUE(memcmp(output.emission, source.emission,
                     sizeof(source.emission)) == 0);
  ASSERT_EQ(output.alpha, 0);
  PASS();
}
static void test_import_explicit_surfaces_and_alias(void) {
  fr_appearance_texel_t defaults = {.roughness = .85f, .normal = {0, 0, 1}};
  fr_appearance_source_t source = {
      .surface = {.color = {.9f, .05f, .2f}, .roughness = .25f,
                  .normal = {.6f, 0, .8f}, .relief = -.2f},
      .emission = {0, 4, 0}, .alpha = .5f, .surface_mask = 7};
  fr_appearance_source_t expected = source;
  ASSERT_TRUE(fr_appearance_import(&source, &defaults, &source));
  ASSERT_TRUE(memcmp(&source, &expected, sizeof(source)) == 0);
  source.surface_mask = FR_APPEARANCE_NORMAL;
  ASSERT_TRUE(fr_appearance_import(&source, &defaults, &source));
  ASSERT_EQ(source.surface.roughness, defaults.roughness);
  ASSERT_EQ(source.surface.relief, defaults.relief);
  ASSERT_EQ(source.surface.normal[0], .6f);
  PASS();
}
static void test_import_rejects_malformed_without_partial_write(void) {
  fr_appearance_texel_t defaults = {.roughness = .85f, .normal = {0, 0, 1}};
  fr_appearance_source_t source = {
      .surface = {.color = {1, 1, 1}, .roughness = .4f,
                  .normal = {0, 0, 1}}, .alpha = 1, .surface_mask = 7};
  fr_appearance_source_t output, before;
  memset(&output, 0xa5, sizeof(output));
  before = output;
  source.emission[2] = NAN;
  ASSERT_TRUE(!fr_appearance_import(&source, &defaults, &output));
  ASSERT_TRUE(memcmp(&output, &before, sizeof(output)) == 0);
  source.emission[2] = 0;
  source.alpha = 2;
  ASSERT_TRUE(!fr_appearance_import(&source, &defaults, &output));
  source.alpha = 1;
  source.surface_mask = 8;
  ASSERT_TRUE(!fr_appearance_import(&source, &defaults, &output));
  source.surface_mask = 7;
  source.surface.normal[2] = 0;
  ASSERT_TRUE(!fr_appearance_import(&source, &defaults, &output));
  source.surface.normal[2] = 1;
  source.surface.roughness = -1;
  ASSERT_TRUE(!fr_appearance_import(&source, &defaults, &output));
  ASSERT_TRUE(memcmp(&output, &before, sizeof(output)) == 0);
  PASS();
}
int main(void) {
  RUN(test_mixed_material_holes_and_visual_scale);
  RUN(test_slot_generations);
  RUN(test_material_namespaces);
  RUN(test_default_and_explicit_color_independence);
  RUN(test_rest_transform);
  RUN(test_explicit_sidecar_ownership_and_recycle);
  RUN(test_import_material_defaults_and_independent_emission);
  RUN(test_import_explicit_surfaces_and_alias);
  RUN(test_import_rejects_malformed_without_partial_write);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
