#include <math.h>
#include <stddef.h>
#include <string.h>

#include "dungeoncraft/chunk.h"
#include "ferrum/renderer/appearance.h"

_Static_assert(sizeof(fr_appearance_source_t) == 64, "Prop source ABI");
_Static_assert(offsetof(fr_appearance_source_t, alpha) == 48, "Prop alpha ABI");
_Static_assert(offsetof(fr_appearance_source_t, surface_mask) == 60,
               "Prop surface mask ABI");

bool fr_appearance_import(const fr_appearance_source_t *source,
                          const fr_appearance_texel_t *defaults,
                          fr_appearance_source_t *output) {
  (void)source;
  (void)defaults;
  (void)output;
  return false;
}

bool fr_appearance_bind(fr_appearance_sidecar_t *s, fr_appearance_owner_t o,
                        fr_appearance_texel_t *t, uint32_t *i, uint8_t *v,
                        uint32_t n) {
  if (!s || !t || !i || !v || !n || !o.visual_scale)
    return false;
  *s = (fr_appearance_sidecar_t){o, t, i, v, n};
  memset(v, 0, n);
  return true;
}
bool fr_appearance_write(fr_appearance_sidecar_t *s, uint32_t t, uint32_t i,
                         fr_appearance_texel_t a) {
  if (!s || t >= s->capacity)
    return false;
  for (uint32_t c = 0; c < 3; c++)
    if (!isfinite(a.color[c]) || a.color[c] < 0 || !isfinite(a.normal[c]))
      return false;
  if (!isfinite(a.roughness) || a.roughness < 0 || a.roughness > 1 ||
      !isfinite(a.relief))
    return false;
  s->texels[t] = a;
  s->owner_indices[t] = i;
  s->valid[t] = 1;
  return true;
}
bool fr_appearance_read(const fr_appearance_sidecar_t *s, uint32_t t,
                        uint64_t i, uint64_t g, uint32_t *p,
                        fr_appearance_texel_t *a) {
  if (!s || !p || !a || t >= s->capacity || !s->valid[t] ||
      !fr_appearance_matches(&s->owner, i, g))
    return false;
  *p = s->owner_indices[t];
  *a = s->texels[t];
  return true;
}

bool fr_appearance_resolve(uint32_t w, uint32_t h, uint32_t s, uint32_t x,
                           uint32_t y, const uint32_t *m, uint32_t *p,
                           uint32_t *a) {
  if (!w || !h || !s || !m || !p || !a || x / s >= w || y / s >= h ||
      (uint64_t)w * h > UINT32_MAX)
    return false;
  uint32_t index = (y / s) * w + x / s;
  if (m[index] == DC_MATERIAL_AIR)
    return false;
  *p = index;
  *a = m[index];
  return true;
}
bool fr_appearance_matches(const fr_appearance_owner_t *o, uint64_t i,
                           uint64_t g) {
  return o && o->identity == i && o->generation == g;
}
bool fr_appearance_material(uint32_t d, uint32_t m, uint32_t *t) {
  static const uint32_t body_materials[] = {
      DC_MATERIAL_AIR, DC_MATERIAL_STONE, DC_MATERIAL_WOOD,
      DC_MATERIAL_MORTAR_DEBRIS, DC_MATERIAL_DENSE_WOOD};
  if (!t)
    return false;
  if (d == FR_APPEARANCE_BODY) {
    if (m >= sizeof(body_materials) / sizeof(body_materials[0]))
      return false;
    *t = body_materials[m];
    return true;
  }
  if (d != FR_APPEARANCE_TERRAIN || m > DC_MATERIAL_DENSE_WOOD)
    return false;
  *t = m;
  return true;
}
fr_appearance_texel_t fr_appearance_default(fr_appearance_owner_t o) {
  uint32_t m = 0;
  fr_appearance_texel_t result = {
      .color = {.2f, .2f, .2f}, .roughness = .85f, .normal = {0, 0, 1}};
  if (!fr_appearance_material(o.domain, o.material, &m))
    return result;
  if (m == DC_MATERIAL_WOOD || m == DC_MATERIAL_DENSE_WOOD) {
    result.color[0] = .35f;
    result.color[1] = .16f;
    result.color[2] = .045f;
  } else if (m == DC_MATERIAL_MORTAR_DEBRIS) {
    result.color[0] = .42f;
    result.color[1] = .39f;
    result.color[2] = .34f;
  }
  return result;
}
void fr_appearance_rest_point(float x, float y, float ox, float oy, float a,
                              float *rx, float *ry) {
  float c = cosf(a), s = sinf(a), dx = x - ox, dy = y - oy;
  if (rx)
    *rx = c * dx + s * dy;
  if (ry)
    *ry = -s * dx + c * dy;
}
