#include "ferrum/renderer/shading.h"

void fr_renderer_shade(fr_appearance_texel_t a, fr_renderer_light_field_t f,
                       const float ambient[3], float hdr[3]) {
  (void)f;
  (void)ambient;
  for (unsigned c = 0; c < 3; c++)
    hdr[c] = a.color[c];
}
fr_appearance_texel_t fr_renderer_rotate_normal(fr_appearance_texel_t a,
                                                float angle) {
  (void)angle;
  return a;
}
uint32_t fr_renderer_pack_display(const float hdr[3], float exposure) {
  (void)hdr;
  (void)exposure;
  return 0xff000000u;
}
