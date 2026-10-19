#ifndef FERRUM_RENDERER_SHADING_H
#define FERRUM_RENDERER_SHADING_H

#include "ferrum/renderer/appearance.h"
#include "ferrum/renderer/lighting_gpu.h"

/** Reference composition: directional diffuse response plus linear ambient. */
void fr_renderer_shade(fr_appearance_texel_t appearance,
                       fr_renderer_light_field_t field, const float ambient[3],
                       float hdr[3]);
/** Rotate a rest-space surface normal around the physical body rotation axis.
 */
fr_appearance_texel_t
fr_renderer_rotate_normal(fr_appearance_texel_t appearance, float angle);
/** Reinhard HDR compression followed by exactly one linear-to-sRGB conversion.
 */
uint32_t fr_renderer_pack_display(const float hdr[3], float exposure);

#endif
