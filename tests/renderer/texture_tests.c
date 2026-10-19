#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/chunk.h"
#include "ferrum/renderer/texture.h"

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

static void test_wood_grain_varies_coherently(void) {
  fr_appearance_owner_t o = {
      .identity = 17, .material = DC_MATERIAL_WOOD, .visual_scale = 1};
  float low = 1, high = 0, along = 0, across = 0;
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 32; x++) {
      o.x = x;
      o.y = y;
      fr_appearance_texel_t a = fr_texture_sample(o, 0);
      if (a.color[0] < low)
        low = a.color[0];
      if (a.color[0] > high)
        high = a.color[0];
      o.x = x + 1;
      fr_appearance_texel_t b = fr_texture_sample(o, 0);
      across += fabsf(a.color[0] - b.color[0]);
      o.x = x;
      o.y = y + 1;
      b = fr_texture_sample(o, 0);
      along += fabsf(a.color[0] - b.color[0]);
    }
  ASSERT_TRUE(high - low > .1f);
  ASSERT_TRUE(across > along * 2);
  PASS();
}
static void test_stone_relief_and_normal(void) {
  fr_appearance_owner_t o = {
      .identity = 91, .material = DC_MATERIAL_STONE, .visual_scale = 1};
  float low = 1, high = 0;
  bool tilted = false;
  for (int x = 0; x < 64; x++) {
    o.x = x;
    o.y = 11;
    fr_appearance_texel_t a = fr_texture_sample(o, 0);
    if (a.color[0] < low)
      low = a.color[0];
    if (a.color[0] > high)
      high = a.color[0];
    float n = a.normal[0] * a.normal[0] + a.normal[1] * a.normal[1] +
              a.normal[2] * a.normal[2];
    ASSERT_TRUE(fabsf(n - 1) < .0001f);
    tilted |= fabsf(a.normal[0]) > .01f;
  }
  ASSERT_TRUE(high - low > .04f);
  ASSERT_TRUE(tilted);
  PASS();
}
static void test_seed_ignores_slot_generation_and_material_namespace(void) {
  fr_appearance_owner_t o = {.identity = 333,
                             .generation = 1,
                             .x = -321,
                             .y = 47,
                             .material = DC_MATERIAL_WOOD,
                             .visual_scale = 1};
  fr_appearance_texel_t a = fr_texture_sample(o, 0);
  o.generation = 99;
  fr_appearance_texel_t b = fr_texture_sample(o, 0);
  ASSERT_TRUE(!memcmp(&a, &b, sizeof(a)));
  o.domain = FR_APPEARANCE_BODY;
  o.material = 2;
  b = fr_texture_sample(o, 0);
  ASSERT_TRUE(!memcmp(&a, &b, sizeof(a)));
  PASS();
}
static void test_brick_cosmetic_style_and_rings(void) {
  fr_appearance_owner_t o = {.identity = 42,
                             .x = 10,
                             .y = 6,
                             .material = DC_MATERIAL_STONE,
                             .visual_scale = 1};
  fr_appearance_texel_t stone = fr_texture_sample(o, 0),
                        brick = fr_texture_sample(o, FR_TEXTURE_BRICK);
  ASSERT_TRUE(memcmp(&stone, &brick, sizeof(stone)) != 0);
  ASSERT_EQ(o.material, DC_MATERIAL_STONE);
  o.material = DC_MATERIAL_WOOD;
  stone = fr_texture_sample(o, 0);
  brick = fr_texture_sample(o, FR_TEXTURE_WOOD_RINGS);
  ASSERT_TRUE(memcmp(&stone, &brick, sizeof(stone)) != 0);
  PASS();
}
static void test_dirt_is_earth_colored_and_distinct_from_stone(void) {
  fr_appearance_owner_t o={.identity=91,.material=DC_MATERIAL_DIRT,.visual_scale=1,.x=12,.y=11};
  fr_appearance_texel_t dirt=fr_texture_sample(o,0);
  ASSERT_TRUE(dirt.color[0]>dirt.color[1]*1.5f);
  ASSERT_TRUE(dirt.color[1]>dirt.color[2]*1.5f);
  o.material=DC_MATERIAL_STONE;
  fr_appearance_texel_t stone=fr_texture_sample(o,0);
  ASSERT_TRUE(fabsf(stone.color[0]-stone.color[1])<.001f);
  PASS();
}
int main(void) {
  RUN(test_dirt_is_earth_colored_and_distinct_from_stone);
  RUN(test_wood_grain_varies_coherently);
  RUN(test_stone_relief_and_normal);
  RUN(test_seed_ignores_slot_generation_and_material_namespace);
  RUN(test_brick_cosmetic_style_and_rings);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
