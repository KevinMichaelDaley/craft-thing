#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ferrum/assets/prop.h"

enum { PROP_HEADER = 64, PROP_FRAME = 16, PROP_MAP = 8, PROP_APPEARANCE = 64,
       PROP_MAX_PIXELS = 4194304, PROP_MAX_FRAMES = 4096 };
_Static_assert(sizeof(float) == 4, "Prop appearance requires 32-bit float");

static uint32_t prop_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static float prop_float(const uint8_t *p) {
    uint32_t bits = prop_u32(p); float value;
    memcpy(&value, &bits, sizeof(value)); return value;
}
static uint32_t prop_crc(const uint8_t *p, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        if (i >= 44 && i < 48) continue;
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
static bool prop_error(char *buffer, uint32_t capacity, const char *message) {
    if (buffer && capacity) snprintf(buffer, capacity, "%s", message);
    return false;
}
uint32_t fr_prop_terrain_material(uint32_t material) {
    static const uint8_t registry[] = {0, 1, 7, 0, 8, 2, 3, 4, 5, 6, 9};
    return material < sizeof(registry) ? registry[material] : 0;
}
uint32_t fr_prop_body_material(uint32_t material) {
    return material == 10 ? 5 : material >= 1 && material <= 4 ? material : 0;
}
bool fr_prop_asset_open(fr_prop_asset_t *asset, const void *bytes, size_t size,
                        char *err_buf, uint32_t err_cap) {
    const uint8_t *p = bytes;
    if (!asset || !p || size < PROP_HEADER || size > UINT32_MAX || memcmp(p, "CPRP", 4) ||
        prop_u32(p + 4) != 1 || prop_u32(p + 8) != size)
        return prop_error(err_buf, err_cap, "Invalid compiled prop header/version/length");
    uint32_t width = prop_u32(p + 12), height = prop_u32(p + 16), frames = prop_u32(p + 20);
    uint64_t pixels = (uint64_t)width * height;
    uint32_t flags = prop_u32(p + 24), data_offset = prop_u32(p + 32);
    uint64_t table_end = PROP_HEADER + (uint64_t)frames * PROP_FRAME;
    uint64_t expected_size = table_end + pixels * (PROP_MAP + PROP_APPEARANCE) * frames;
    if (!width || !height || pixels > PROP_MAX_PIXELS || !frames || frames > PROP_MAX_FRAMES ||
        pixels * frames > PROP_MAX_PIXELS * 16ull || flags > 1 || prop_u32(p + 28) != PROP_HEADER ||
        data_offset != table_end || prop_u32(p + 36) != PROP_MAP ||
        prop_u32(p + 40) != PROP_APPEARANCE || expected_size != size)
        return prop_error(err_buf, err_cap, "Invalid compiled prop dimensions/layout");
    if (prop_crc(p, size) != prop_u32(p + 44))
        return prop_error(err_buf, err_cap, "Compiled prop CRC mismatch");
    for (uint32_t frame = 0; frame < frames; ++frame) {
        const uint8_t *entry = p + PROP_HEADER + frame * PROP_FRAME;
        uint32_t duration = prop_u32(entry);
        uint64_t frame_offset = (uint64_t)frame * pixels * (PROP_MAP + PROP_APPEARANCE);
        if (!duration || duration > 65535 || prop_u32(entry + 4) != frame_offset ||
            prop_u32(entry + 8) != pixels * PROP_MAP || prop_u32(entry + 12) != pixels * PROP_APPEARANCE)
            return prop_error(err_buf, err_cap, "Invalid compiled prop frame table");
        const uint8_t *map = p + data_offset + frame_offset;
        if (!flags && frame && memcmp(map, p + data_offset, (size_t)pixels * PROP_MAP))
            return prop_error(err_buf, err_cap, "Physical metadata changes under visual-only policy");
        for (uint32_t pixel = 0; pixel < pixels; ++pixel) {
            const uint8_t *record = map + pixel * PROP_MAP;
            uint32_t material = (uint32_t)record[0] | (uint32_t)record[1] << 8;
            uint32_t role = record[2];
            if (material > 10 || role > 4 || record[3] > 1 || prop_u32(record + 4) > 0xffffff ||
                (record[3] && role != 2) || (!material && role >= 1 && role <= 3) ||
                (material && (role < 1 || role > 3 ||
                 !(role == 1 ? fr_prop_terrain_material(material) : fr_prop_body_material(material)))))
                return prop_error(err_buf, err_cap, "Incompatible compiled material/role/object/anchor");
            const uint8_t *app = map + pixels * PROP_MAP + pixel * PROP_APPEARANCE;
            if (prop_u32(app + 60) > 7)
                return prop_error(err_buf, err_cap, "Unknown surface override flags");
            if (prop_u32(app + 44) || prop_u32(app + 52) || prop_u32(app + 56))
                return prop_error(err_buf, err_cap, "Nonzero reserved appearance words");
            for (uint32_t component = 0; component < 15; ++component) {
                float value = prop_float(app + component * 4);
                if (!isfinite(value) || ((component <= 3 || component >= 7) && value < 0) ||
                    ((component == 3 || component == 12) && value > 1) ||
                    (component >= 4 && component <= 6 && (value < -1 || value > 1)))
                    return prop_error(err_buf, err_cap, "Invalid compiled appearance value");
            }
            if (prop_u32(app + 60) & 1u) {
                float x = prop_float(app + 16), y = prop_float(app + 20);
                float z = prop_float(app + 24);
                if (fabsf(x * x + y * y + z * z - 1.0f) > 0.001f)
                    return prop_error(err_buf, err_cap, "Explicit normal must have unit length");
            }
        }
    }
    fr_prop_asset_t candidate = {p, size, width, height, frames, flags, data_offset};
    *asset = candidate;
    if (err_buf && err_cap) err_buf[0] = '\0';
    return true;
}
static const uint8_t *prop_record(const fr_prop_asset_t *asset, uint32_t frame, uint32_t pixel) {
    if (!asset || !asset->bytes || frame >= asset->frame_count || pixel >= asset->width * asset->height)
        return NULL;
    uint32_t offset = prop_u32(asset->bytes + PROP_HEADER + frame * PROP_FRAME + 4);
    return asset->bytes + asset->data_offset + offset + pixel * PROP_MAP;
}
bool fr_prop_pixel(const fr_prop_asset_t *asset, uint32_t frame, uint32_t pixel,
                   fr_prop_pixel_t *out) {
    const uint8_t *p = prop_record(asset, frame, pixel);
    if (!p || !out) return false;
    *out = (fr_prop_pixel_t){(uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8), p[2], p[3], prop_u32(p + 4)};
    return true;
}
bool fr_prop_appearance(const fr_prop_asset_t *asset, uint32_t frame, uint32_t pixel,
                        fr_prop_appearance_t *out) {
    const uint8_t *record = prop_record(asset, frame, pixel);
    if (!record || !out) return false;
    const uint8_t *p = record - pixel * PROP_MAP + asset->width * asset->height * PROP_MAP + pixel * PROP_APPEARANCE;
    for (uint32_t i = 0; i < 3; ++i) {
        out->color[i] = prop_float(p + i * 4);
        out->normal[i] = prop_float(p + 16 + i * 4);
        out->emission[i] = prop_float(p + 32 + i * 4);
    }
    out->roughness = prop_float(p + 12); out->relief = prop_float(p + 28);
    out->alpha = prop_float(p + 48);
    out->surface_mask = prop_u32(p + 60);
    return true;
}
uint32_t fr_prop_animation_frame(const fr_prop_asset_t *asset, uint64_t elapsed_ms) {
    if (!asset || !asset->bytes || !asset->frame_count) return 0;
    uint64_t total = 0;
    for (uint32_t frame = 0; frame < asset->frame_count; ++frame)
        total += prop_u32(asset->bytes + PROP_HEADER + frame * PROP_FRAME);
    elapsed_ms %= total;
    for (uint32_t frame = 0; frame < asset->frame_count; ++frame) {
        uint32_t duration = prop_u32(asset->bytes + PROP_HEADER + frame * PROP_FRAME);
        if (elapsed_ms < duration) return frame;
        elapsed_ms -= duration;
    }
    return 0;
}
