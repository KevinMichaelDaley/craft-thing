#include <stdio.h>
#include "gpu_internal.h"

bool dc_gpu_read_pixel_shape(dc_gpu_t *gpu, uint32_t id,
                             dc_gpu_body_pixel_t *pixels, uint32_t capacity,
                             uint32_t *count, char *err, uint32_t cap) {
    if (gpu && id && count) {
        dc_gpu_refresh_body_registry(gpu);
        const dc_gpu_body_record_t *bodies = gpu->body_mapped;
        for (unsigned slot = 0; slot < DC_GPU_BODY_CAPACITY; slot++) {
            const dc_gpu_body_record_t *body = bodies + slot;
            if (!body->body.active || body->body.id != id)
                continue;
            if (!(body->piece_count & DC_GPU_PIXEL_BODY_FLAG))
                break;
            *count = body->piece_count & ~DC_GPU_PIXEL_BODY_FLAG;
            if (!pixels)
                return true;
            if (capacity < *count)
                break;
            uint64_t area = (uint64_t)body->body.width * body->body.height;
            uint64_t words = (area + 7) / 8;
            if (!area ||
                body->compound_reserved < dc_gpu_pixel_pool_word_offset(gpu) +
                                              DC_GPU_PIXEL_POOL_HEADER_WORDS ||
                body->compound_reserved + words >
                    dc_gpu_body_storage_bytes(gpu) / 4)
                break;
            const uint32_t *packed =
                (const uint32_t *)gpu->body_mapped + body->compound_reserved;
            unsigned output = 0;
            for (uint64_t index = 0; index < area; index++) {
                uint32_t material =
                    (packed[index / 8] >> ((index % 8) * 4)) & 15u;
                if (!material)
                    continue;
                if (output >= *count)
                    break;
                pixels[output++] =
                    (dc_gpu_body_pixel_t){index % body->body.width,
                                          index / body->body.width, material};
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
