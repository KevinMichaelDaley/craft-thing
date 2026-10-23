#include <stdio.h>
#include <stdlib.h>
#include "dungeoncraft/material_density.h"
#include "gpu_internal.h"

bool dc_gpu_pixel_geometry_valid(const dc_gpu_body_record_t *body,
                                 const uint32_t *geometry, uint32_t words,
                                 float *density) {
    if (!geometry || words < 8 || !geometry[1] ||
        geometry[2] != ((uint64_t)body->body.width + 15) / 16 ||
        geometry[3] != ((uint64_t)body->body.height + 15) / 16 ||
        geometry[2] > 65536 || geometry[3] > 65536)
        return false;
    uint64_t directory = (uint64_t)geometry[2] * geometry[3];
    uint64_t length = 8 + directory + (uint64_t)geometry[1] * 33;
    if (length > words || geometry[0] != length)
        return false;
    for (uint64_t i = 0; i < directory; i++) {
        uint32_t offset = geometry[8 + i];
        if (offset && (offset < 8 + directory || offset >= length ||
                       (offset - 8 - directory) % 33))
            return false;
        if (offset && ((geometry[offset] & 65535u) != i % geometry[2] ||
                       (geometry[offset] >> 16) != i / geometry[2]))
            return false;
    }
    uint32_t count = 0;
    uint32_t low_x = UINT32_MAX, low_y = UINT32_MAX, high_x = 0, high_y = 0;
    double mass = 0;
    for (uint32_t tile = 0; tile < geometry[1]; tile++) {
        uint64_t offset = 8 + directory + (uint64_t)tile * 33;
        uint32_t x = geometry[offset] & 65535u, y = geometry[offset] >> 16;
        if (x >= geometry[2] || y >= geometry[3] ||
            geometry[8 + (uint64_t)y * geometry[2] + x] != offset)
            return false;
        unsigned occupied = 0;
        for (unsigned p = 0; p < 256; p++) {
            unsigned material =
                (geometry[offset + 1 + p / 8] >> ((p % 8) * 4)) & 15u;
            if (material > DC_BODY_MATERIAL_MAX ||
                (material && (x * 16 + p % 16 >= body->body.width ||
                              y * 16 + p / 16 >= body->body.height)))
                return false;
            if (!material)
                continue;
            uint32_t px = x * 16 + p % 16, py = y * 16 + p / 16;
            if (px < low_x)
                low_x = px;
            if (py < low_y)
                low_y = py;
            if (px + 1 > high_x)
                high_x = px + 1;
            if (py + 1 > high_y)
                high_y = py + 1;
            occupied++;
            mass += DC_BODY_DENSITY(material);
        }
        if (!occupied)
            return false;
        count += occupied;
    }
    if (!count || count != (body->piece_count & ~DC_GPU_PIXEL_BODY_FLAG) ||
        geometry[4] != low_x || geometry[5] != low_y || geometry[6] != high_x ||
        geometry[7] != high_y)
        return false;
    *density = (float)(mass / count);
    return true;
}

static uint32_t dense_material(const uint32_t *pixels, uint32_t width,
                               uint32_t x, uint32_t y) {
    uint64_t index = (uint64_t)y * width + x;
    return (pixels[index / 8] >> ((index % 8) * 4)) & 15u;
}

bool dc_gpu_upgrade_dense_pixels(dc_gpu_t *gpu, dc_gpu_body_record_t *bodies,
                                 uint32_t **pixels, uint32_t *words) {
    uint32_t capacity = dc_gpu_pixel_pool_capacity(gpu), used = 0;
    uint32_t *converted = calloc(capacity, sizeof(*converted));
    if (!converted)
        return false;
    for (unsigned slot = 0; slot < DC_GPU_BODY_CAPACITY; slot++) {
        dc_gpu_body_record_t *b = bodies + slot;
        if ((!b->body.active && !b->reserved[0]) ||
            !(b->piece_count & DC_GPU_PIXEL_BODY_FLAG))
            continue;
        uint64_t area = (uint64_t)b->body.width * b->body.height;
        uint64_t length = (area + 7) / 8;
        uint64_t dw = ((uint64_t)b->body.width + 15) / 16;
        uint64_t dh = ((uint64_t)b->body.height + 15) / 16;
        if (!area || b->compound_reserved > *words ||
            length > *words - b->compound_reserved || dw > 65536 ||
            dh > 65536 || 8 + dw * dh > capacity - used)
            goto invalid;
        const uint32_t *dense = *pixels + b->compound_reserved;
        uint32_t count = 0;
        uint32_t low_x = UINT32_MAX, low_y = UINT32_MAX, high_x = 0, high_y = 0;
        for (uint64_t p = 0; p < area; p++) {
            uint32_t material = (dense[p / 8] >> ((p % 8) * 4)) & 15u;
            if (material > DC_BODY_MATERIAL_MAX)
                goto invalid;
            count += material != 0;
            if (material) {
                uint32_t x = p % b->body.width, y = p / b->body.width;
                if (x < low_x)
                    low_x = x;
                if (y < low_y)
                    low_y = y;
                if (x + 1 > high_x)
                    high_x = x + 1;
                if (y + 1 > high_y)
                    high_y = y + 1;
            }
        }
        if (!count || count != (b->piece_count & ~DC_GPU_PIXEL_BODY_FLAG))
            goto invalid;
        uint32_t *shape = converted + used;
        shape[2] = dw;
        shape[3] = dh;
        shape[4] = low_x;
        shape[5] = low_y;
        shape[6] = high_x;
        shape[7] = high_y;
        uint32_t output = 8 + dw * dh, tiles = 0;
        for (uint32_t ty = 0; ty < dh; ty++)
            for (uint32_t tx = 0; tx < dw; tx++) {
                bool occupied = false;
                uint32_t packed[32] = {0};
                for (unsigned p = 0; p < 256; p++) {
                    uint32_t x = tx * 16 + p % 16, y = ty * 16 + p / 16;
                    if (x >= b->body.width || y >= b->body.height)
                        continue;
                    uint32_t material =
                        dense_material(dense, b->body.width, x, y);
                    packed[p / 8] |= material << ((p % 8) * 4);
                    occupied |= material != 0;
                }
                if (!occupied)
                    continue;
                if ((uint64_t)output + 33 > capacity - used)
                    goto invalid;
                shape[8 + ty * dw + tx] = output;
                shape[output] = tx | (ty << 16);
                for (unsigned word = 0; word < 32; word++)
                    shape[output + 1 + word] = packed[word];
                output += 33;
                tiles++;
            }
        shape[0] = output;
        shape[1] = tiles;
        b->compound_reserved = used;
        used += output;
    }
    free(*pixels);
    *pixels = converted;
    *words = used;
    return true;
invalid:
    free(converted);
    return false;
}

bool dc_gpu_read_pixel_shape(dc_gpu_t *gpu, uint32_t id,
                             dc_gpu_body_pixel_t *pixels, uint32_t capacity,
                             uint32_t *count, char *err, uint32_t cap) {
    if (gpu && id && count) {
        dc_gpu_refresh_body_registry(gpu);
        for (unsigned slot = 0; slot < dc_gpu_body_capacity(gpu); slot++) {
            const dc_gpu_body_record_t *body = dc_gpu_body_record(gpu,slot);
            if (!body->body.active || body->body.id != id)
                continue;
            if (!(body->piece_count & DC_GPU_PIXEL_BODY_FLAG))
                break;
            *count = body->piece_count & ~DC_GPU_PIXEL_BODY_FLAG;
            if (body->compound_reserved < dc_gpu_pixel_pool_word_offset(gpu) +
                                              DC_GPU_PIXEL_POOL_HEADER_WORDS ||
                (uint64_t)body->compound_reserved + 8 >
                    dc_gpu_body_storage_bytes(gpu) / 4)
                break;
            const uint32_t *packed =
                (const uint32_t *)gpu->body_mapped + body->compound_reserved;
            float density;
            if (!dc_gpu_pixel_geometry_valid(body, packed,
                                             dc_gpu_body_storage_bytes(gpu) /
                                                     4 -
                                                 body->compound_reserved,
                                             &density))
                break;
            if (!pixels)
                return true;
            if (capacity < *count)
                break;
            unsigned output = 0;
            uint32_t directory = packed[2] * packed[3];
            for (unsigned tile = 0; tile < packed[1]; tile++) {
                uint32_t offset = 8 + directory + tile * 33;
                uint32_t x = (packed[offset] & 65535u) * 16,
                         y = (packed[offset] >> 16) * 16;
                for (unsigned p = 0; p < 256; p++) {
                    uint32_t material =
                        (packed[offset + 1 + p / 8] >> ((p % 8) * 4)) & 15u;
                    if (material)
                        pixels[output++] = (dc_gpu_body_pixel_t){
                            x + p % 16, y + p / 16, material};
                }
            }
            if (output == *count)
                return true;
            break;
        }
    }
    if (err && cap)
        snprintf(err, cap,
                 "Invalid pixel shape or insufficient output capacity");
    return false;
}
