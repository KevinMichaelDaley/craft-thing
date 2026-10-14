#include "ferrum/renderer/texture.h"

fr_appearance_texel_t fr_texture_sample(fr_appearance_owner_t owner, uint32_t style) {
    (void)style;return fr_appearance_default(owner);
}
