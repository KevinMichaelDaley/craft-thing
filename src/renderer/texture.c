#include <math.h>

#include "dungeoncraft/chunk.h"
#include "ferrum/renderer/texture.h"

static uint32_t texture_hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}
static float noise_corner(int x, int y, uint32_t seed) {
  uint32_t hash = texture_hash((uint32_t)x * 0x9e3779b9u ^
                               (uint32_t)y * 0x85ebca6bu ^ seed);
  return (float)(hash & 0xffffu) / 65535.f;
}
static float smooth_noise(float x, float y, uint32_t seed) {
  int ix = (int)floorf(x), iy = (int)floorf(y);
  float fx = x - ix, fy = y - iy;
  fx = fx * fx * (3 - 2 * fx);
  fy = fy * fy * (3 - 2 * fy);
  float a = noise_corner(ix, iy, seed), b = noise_corner(ix + 1, iy, seed);
  float c = noise_corner(ix, iy + 1, seed),
        d = noise_corner(ix + 1, iy + 1, seed);
  return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
}
static float surface_height(float x, float y, uint32_t seed, uint32_t material,
                            uint32_t style) {
  float phase = (float)(seed & 1023u) * .006135923f;
  if (material == DC_MATERIAL_WOOD || material == DC_MATERIAL_DENSE_WOOD) {
    float warped = x + .7f * sinf(y * .035f + phase) + .3f * sinf(y * .08f);
    float grain = style == FR_TEXTURE_WOOD_RINGS ? hypotf(x + 14, y + 8) * .62f
                                                 : warped * .75f;
    return .5f + .5f * sinf(grain + phase);
  }
  if (style == FR_TEXTURE_BRICK) {
    float row = floorf(y / 8.f), bx = x + (fmodf(row, 2.f) == 0 ? 0 : 8);
    float joint_x = bx - floorf(bx / 16.f) * 16, joint_y = y - row * 8;
    if (joint_x < 1 || joint_y < 1)
      return .08f;
    return .55f + .3f * smooth_noise(x * .13f, y * .13f, seed);
  }
  return .7f * smooth_noise(x * .12f, y * .12f, seed) +
         .3f * smooth_noise(x * .47f, y * .47f, seed + 31);
}

fr_appearance_texel_t fr_texture_sample(fr_appearance_owner_t owner,
                                        uint32_t style) {
  fr_appearance_texel_t result = fr_appearance_default(owner);
  uint32_t material;
  if (!fr_appearance_material(owner.domain, owner.material, &material) ||
      material == 0)
    return result;
  uint32_t seed =
      texture_hash((uint32_t)owner.identity ^ (uint32_t)(owner.identity >> 32));
  float x = (float)(owner.x % 1048576), y = (float)(owner.y % 1048576);
  float h = surface_height(x, y, seed, material, style);
  float variation = .65f + .7f * h;
  if (style == FR_TEXTURE_BRICK && material != DC_MATERIAL_WOOD &&
      material != DC_MATERIAL_DENSE_WOOD) {
    result.color[0] = h < .1f ? .33f : .42f;
    result.color[1] = h < .1f ? .31f : .12f;
    result.color[2] = h < .1f ? .28f : .055f;
  }
  for (uint32_t c = 0; c < 3; c++)
    result.color[c] *= variation;
  float strength = material == DC_MATERIAL_STONE ? .55f : .2f;
  float nx = -strength * (surface_height(x + 1, y, seed, material, style) -
                          surface_height(x - 1, y, seed, material, style));
  float ny = -strength * (surface_height(x, y + 1, seed, material, style) -
                          surface_height(x, y - 1, seed, material, style));
  float inv = 1 / sqrtf(nx * nx + ny * ny + 1);
  result.normal[0] = nx * inv;
  result.normal[1] = ny * inv;
  result.normal[2] = inv;
  result.relief = h * strength;
  result.roughness = .7f + .25f * (1 - h);
  return result;
}
