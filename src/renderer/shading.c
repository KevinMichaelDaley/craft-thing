#include <math.h>

#include "ferrum/renderer/shading.h"

void fr_renderer_shade(fr_appearance_texel_t a, fr_renderer_light_field_t f,
                       const float ambient[3], float hdr[3]) {
  float contrast = 1 - .35f * a.roughness;
  for (unsigned c = 0; c < 3; c++) {
    float directional =
        a.normal[0] * f.lx[c] + a.normal[1] * f.ly[c] + a.normal[2] * f.lz[c];
    float light = fmaxf(0, .5f * (f.l0[c] + contrast * directional));
    hdr[c] = a.color[c] * (light + ambient[c]);
  }
}
fr_appearance_texel_t fr_renderer_rotate_normal(fr_appearance_texel_t a,
                                                float angle) {
  float c = cosf(angle), s = sinf(angle), x = a.normal[0], y = a.normal[1];
  a.normal[0] = c * x - s * y;
  a.normal[1] = s * x + c * y;
  return a;
}
uint32_t fr_renderer_pack_display(const float hdr[3], float exposure) {
  uint32_t packed = 0xff000000u;
  for (unsigned c = 0; c < 3; c++) {
    float linear = fmaxf(0, hdr[c] * exposure);
    linear = linear / (1 + linear);
    float srgb = linear <= .0031308f ? 12.92f * linear
                                     : 1.055f * powf(linear, 1 / 2.4f) - .055f;
    packed |= (uint32_t)lroundf(fminf(1, fmaxf(0, srgb)) * 255) << (8 * c);
  }
  return packed;
}
