#include "ferrum/assets/prop.h"

bool fr_prop_asset_open(fr_prop_asset_t *asset, const void *bytes, size_t size,
                        char *err_buf, uint32_t err_cap) {
    (void)asset; (void)bytes; (void)size; (void)err_buf; (void)err_cap;
    return false;
}
bool fr_prop_pixel(const fr_prop_asset_t *asset, uint32_t frame, uint32_t pixel,
                   fr_prop_pixel_t *out) {
    (void)asset; (void)frame; (void)pixel; (void)out; return false;
}
bool fr_prop_appearance(const fr_prop_asset_t *asset, uint32_t frame, uint32_t pixel,
                        fr_prop_appearance_t *out) {
    (void)asset; (void)frame; (void)pixel; (void)out; return false;
}
uint32_t fr_prop_terrain_material(uint32_t material) { (void)material; return 0; }
uint32_t fr_prop_body_material(uint32_t material) { (void)material; return 0; }
uint32_t fr_prop_animation_frame(const fr_prop_asset_t *asset, uint64_t elapsed_ms) {
    (void)asset; (void)elapsed_ms; return 0;
}
