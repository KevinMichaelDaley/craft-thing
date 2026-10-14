#ifndef FERRUM_RENDERER_TEXTURE_H
#define FERRUM_RENDERER_TEXTURE_H

#include "ferrum/renderer/appearance.h"

enum {
  FR_TEXTURE_DEFAULT = 0,
  FR_TEXTURE_WOOD_RINGS = 1,
  FR_TEXTURE_BRICK = 2
};

/** Reference sample keyed by immutable ownership; cosmetic styles never change
 * material. */
fr_appearance_texel_t fr_texture_sample(fr_appearance_owner_t owner,
                                        uint32_t style);

#endif
