#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/material_density.h"
#include "ferrum/fire/gpu.h"
#include "reactive_storage_gpu.h"
#include "runtime_body_internal.h"

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

static bool pixel_dense_layout(const dc_gpu_body_record_t *b,uint32_t words,
                               uint64_t *area,uint64_t *dw,uint64_t *dh) {
    *area=(uint64_t)b->body.width*b->body.height;
    *dw=((uint64_t)b->body.width+15u)/16u;
    *dh=((uint64_t)b->body.height+15u)/16u;
    return *area&&b->compound_reserved<=words&&(*area+7u)/8u<=words-b->compound_reserved&&
        *dw<=65536u&&*dh<=65536u;
}
bool dc_gpu_upgrade_dense_pixels(dc_gpu_t *gpu, dc_gpu_body_record_t *bodies,
                                 uint32_t **pixels, uint32_t *words) {
    uint32_t capacity = dc_gpu_pixel_pool_capacity(gpu), used = 0;
    uint64_t required=0;
    for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++) {
        const dc_gpu_body_record_t *b=bodies+slot;uint64_t area,dw,dh;
        if((!b->body.active&&!b->reserved[0])||!(b->piece_count&DC_GPU_PIXEL_BODY_FLAG)) continue;
        if(!pixel_dense_layout(b,*words,&area,&dw,&dh)) return false;
        required+=8u+dw*dh*34u;
        if(required>UINT32_MAX) return false;
    }
    if(required>capacity) capacity=(uint32_t)required;
    uint32_t *converted = calloc(capacity, sizeof(*converted));
    if (!converted)
        return false;
    for (unsigned slot = 0; slot < DC_GPU_BODY_CAPACITY; slot++) {
        dc_gpu_body_record_t *b = bodies + slot;
        if ((!b->body.active && !b->reserved[0]) ||
            !(b->piece_count & DC_GPU_PIXEL_BODY_FLAG))
            continue;
        uint64_t area,dw,dh;
        if (!pixel_dense_layout(b,*words,&area,&dw,&dh)||8+dw*dh>capacity-used)
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

enum {
    PIXEL_BIRTH_SIDE = 16,
    PIXEL_BIRTH_CELLS = PIXEL_BIRTH_SIDE * PIXEL_BIRTH_SIDE,
    PIXEL_BIRTH_TILE = 9,
    PIXEL_BIRTH_MATERIALS = PIXEL_BIRTH_TILE + 1,
    PIXEL_BIRTH_WORDS = PIXEL_BIRTH_MATERIALS + PIXEL_BIRTH_CELLS / 8
};

static bool pixel_birth_error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static bool pixel_birth_footprint(dc_gpu_t *g, const dc_gpu_world_body_t *initial,
    const uint16_t *rows, uint32_t *grid_x, uint32_t *grid_y,
    char *err, uint32_t cap) {
    uint32_t slot = dc_gpu_find_active_chunk(g, initial->chunk);
    if (slot == UINT32_MAX)
        return pixel_birth_error(err, cap, "Pixel body anchor is not resident");
    uint32_t tile = g->slot_page[slot];
    uint32_t tx = tile % g->page_width, ty = tile / g->page_width;
    uint32_t x = tx * DC_CHUNK_SIDE + (uint32_t)(initial->body.x_fp / DC_FLUID_FULL);
    uint32_t y = ty * DC_CHUNK_SIDE + (uint32_t)(initial->body.y_fp / DC_FLUID_FULL);
    if (x >= g->width || y >= g->height || initial->body.width > g->width - x ||
        initial->body.height > g->height - y)
        return pixel_birth_error(err, cap, "Pixel body extends outside active grid");
    uint32_t last_x = (x + initial->body.width - 1u) / DC_CHUNK_SIDE;
    uint32_t last_y = (y + initial->body.height - 1u) / DC_CHUNK_SIDE;
    if (g->body_origin.x > INT64_MAX - last_x ||
        g->body_origin.y > INT64_MAX - last_y ||
        g->body_origin.x + tx != initial->chunk.x ||
        g->body_origin.y + ty != initial->chunk.y)
        return pixel_birth_error(err, cap, "Pixel body anchor does not match the physics pages");
    if (!dc_gpu_set_body_origin(g, g->body_origin, err, cap)) return false;

    /* A one-tile shape can cover up to four streamed chunks. */
    uint32_t loaded[4], loaded_count = 0;
    for (uint32_t py = 0; py < initial->body.height; ++py)
        for (uint32_t px = 0; px < initial->body.width; ++px) {
            if (!(rows[py] & (1u << px))) continue;
            uint32_t cell_x = x + px, cell_y = y + py;
            uint32_t page_x = cell_x / DC_CHUNK_SIDE, page_y = cell_y / DC_CHUNK_SIDE;
            dc_chunk_coord_t coord = {g->body_origin.x + page_x, g->body_origin.y + page_y};
            uint32_t page_slot = dc_gpu_find_active_chunk(g, coord);
            if (page_slot == UINT32_MAX || g->slot_page[page_slot] != page_y * g->page_width + page_x)
                return pixel_birth_error(err, cap, "Pixel body crosses an unmapped chunk");
            uint32_t cached = 0;
            while (cached < loaded_count && loaded[cached] != page_slot) ++cached;
            if (cached == loaded_count) {
                if (!dc_gpu_copy_chunk_state(g, page_slot, false, false, err, cap))
                    return false;
                loaded[loaded_count++] = page_slot;
            }
            uint32_t local = cell_y % DC_CHUNK_SIDE * DC_CHUNK_SIDE + cell_x % DC_CHUNK_SIDE;
            const dc_cell_t *cells = g->chunk_mapped;
            const dc_cell_t *cell = &cells[(size_t)page_slot * DC_CHUNK_CELLS + local];
            uint32_t occupant;
            if (!dc_gpu_read_occupancy(g, cell_x, cell_y, &occupant, NULL, err, cap))
                return false;
            if (cell->material != DC_MATERIAL_AIR || cell->fluid_mass || occupant)
                return pixel_birth_error(err, cap, "Pixel body footprint is occupied");
        }
    *grid_x = x;
    *grid_y = y;
    return true;
}

static bool pixel_birth_slot(dc_gpu_t *g, uint32_t *slot, char *err, uint32_t cap) {
    uint32_t capacity = dc_gpu_body_capacity(g);
    for (uint32_t i = 0; i < capacity; ++i)
        if (!g->body_ids[i] && dc_gpu_body_owner(g, i)->state == DC_BODY_OWNER_FREE &&
            dc_gpu_body_owner(g, i)->generation != UINT32_MAX) {
            *slot = i;
            return true;
        }
    if (!dc_gpu_runtime_bodies_reserve(g, capacity + 1u, err, cap)) return false;
    *slot = capacity;
    return true;
}

static void pixel_birth_bounds(dc_gpu_body_record_t *record, uint32_t x, uint32_t y) {
    record->bounds_low[0] = record->swept_low[0] = (int32_t)x;
    record->bounds_low[1] = record->swept_low[1] = (int32_t)y;
    record->bounds_high[0] = record->swept_high[0] = (int32_t)(x + record->body.width);
    record->bounds_high[1] = record->swept_high[1] = (int32_t)(y + record->body.height);
}

bool fr_gpu_spawn_burning_pixels(dc_gpu_t *g, dc_gpu_world_body_t initial,
    const uint16_t *rows, uint32_t material, float temperature_K,
    uint32_t *body_id, char *err, uint32_t cap) {
    if (body_id) *body_id = 0;
    dc_gpu_body_t body = initial.body;
    if (!g || !g->runtime_body_consumers || !rows || !body_id || body.id ||
        !body.width || body.width > PIXEL_BIRTH_SIDE ||
        !body.height || body.height > PIXEL_BIRTH_SIDE ||
        body.x_fp < 0 || body.x_fp >= (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL) ||
        body.y_fp < 0 || body.y_fp >= (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL) ||
        body.x_fp % DC_FLUID_FULL || body.y_fp % DC_FLUID_FULL ||
        material == 0u || material > DC_BODY_MATERIAL_MAX)
        return pixel_birth_error(err, cap, "Invalid burning pixel body");

    uint32_t geometry[PIXEL_BIRTH_WORDS] = {
        PIXEL_BIRTH_WORDS, 1, 1, 1, 0, 0, body.width, body.height, PIXEL_BIRTH_TILE
    };
    uint32_t columns = 0, count = 0;
    double sum_x = 0., sum_y = 0., second_moment = 0.;
    uint16_t texels[PIXEL_BIRTH_CELLS];
    for (uint32_t y = 0; y < body.height; ++y) {
        if (rows[y] >> body.width)
            return pixel_birth_error(err, cap, "Pixel row exceeds its declared width");
        columns |= rows[y];
        for (uint32_t x = 0; x < body.width; ++x) {
            if (!(rows[y] & (1u << x))) continue;
            uint32_t pixel = y * PIXEL_BIRTH_SIDE + x;
            geometry[PIXEL_BIRTH_MATERIALS + pixel / 8u] |= material << ((pixel % 8u) * 4u);
            texels[count++] = (uint16_t)pixel;
            double cx = x + .5, cy = y + .5;
            sum_x += cx;
            sum_y += cy;
            second_moment += cx * cx + cy * cy + 1. / 6.;
        }
    }
    if (!(columns & 1u) || !(columns & (1u << (body.width - 1u))) ||
        !rows[0] || !rows[body.height - 1u])
        return pixel_birth_error(err, cap, "Pixel body bounds must be trimmed to occupied texels");
    uint32_t grid_x, grid_y;
    if (!pixel_birth_footprint(g, &initial, rows, &grid_x, &grid_y, err, cap) ||
        !fr_gpu_reactive_initialize(g, err, cap)) return false;
    dc_gpu_refresh_body_registry(g);
    uint32_t id = dc_gpu_next_body_id(g), slot;
    if (!id) return pixel_birth_error(err, cap, "No runtime identity for burning pixel body");
    if (!pixel_birth_slot(g, &slot, err, cap)) return false;

    uint32_t pool_offset = dc_gpu_pixel_pool_word_offset(g);
    uint32_t used = ((const uint32_t *)g->body_mapped)[pool_offset + 1u];
    if (used > UINT32_MAX - PIXEL_BIRTH_WORDS)
        return pixel_birth_error(err, cap, "Pixel geometry address exceeds its range");
    if (!dc_gpu_runtime_geometry_reserve(g, used + PIXEL_BIRTH_WORDS, err, cap))
        return false;
    struct fr_reactive_gpu *reactive = g->reactive;
    if (reactive->high_id >= UINT64_MAX - count)
        return pixel_birth_error(err, cap, "Burning parcel identities exceed their range");
    uint32_t generation = dc_gpu_body_owner(g, slot)->generation + 1u;
    double density = DC_BODY_DENSITY(material);
    uint32_t cell_mass = (uint32_t)llround(density * DC_FLUID_FULL);
    fr_reactive_arena_request_t requests[PIXEL_BIRTH_CELLS];
    fr_reactive_handle_t handles[PIXEL_BIRTH_CELLS];
    uint64_t parcel_id = reactive->high_id;
    for (uint32_t i = 0; i < count; ++i) {
        requests[i] = (fr_reactive_arena_request_t){.operation = FR_REACTIVE_ARENA_ALLOCATE,
            .record.owner = {FR_REACTIVE_OWNER_BODY,
                {id, generation, texels[i] % PIXEL_BIRTH_SIDE, texels[i] / PIXEL_BIRTH_SIDE, 0}}};
        fr_reactive_state_t *state = &requests[i].record.state;
        if (!fr_reactive_state_default(DC_BODY_TO_TERRAIN(material), cell_mass, 0,
                                        temperature_K, ++parcel_id, state, err, cap))
            return false;
        if (!state->fuel_mass_fp)
            return pixel_birth_error(err, cap, "Pixel body material has no combustible fuel");
        state->flags |= FR_REACTIVE_FLAMING;
        state->ignition_integral = state->ignition_threshold;
    }
    uint32_t demand;
    if (!fr_reactive_arena_apply(g, &reactive->arena, requests, count, handles, &demand, err, cap))
        return false;

    /* The completed arena batch is followed by infallible publication. */
    pool_offset = dc_gpu_pixel_pool_word_offset(g);
    uint32_t *pool = (uint32_t *)g->body_mapped + pool_offset;
    uint32_t bank = pool[0], capacity = dc_gpu_pixel_pool_capacity(g);
    uint32_t offset = pool_offset + DC_GPU_PIXEL_POOL_HEADER_WORDS + bank * capacity + used;
    memcpy((uint32_t *)g->body_mapped + offset, geometry, sizeof(geometry));
    memset(dc_gpu_shape_colors(g, offset), 0, PIXEL_BIRTH_WORDS * 128u);
    uint32_t allocated = ((const uint32_t *)reactive->arena.root.mapped)[12];
    uint32_t *bindings = (uint32_t *)reactive->shapes.mapped +
                         ((size_t)bank * allocated + used) * 16u;
    memset(bindings, 0, PIXEL_BIRTH_WORDS * 16u * sizeof(*bindings));
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t at = (PIXEL_BIRTH_MATERIALS * 8u + texels[i]) * 2u;
        bindings[at] = handles[i].index;
        bindings[at + 1u] = handles[i].generation;
    }
    pool[1] = used + PIXEL_BIRTH_WORDS;
    pool[2] = capacity;
    reactive->high_id = parcel_id;

    double center_x = sum_x / count, center_y = sum_y / count;
    float mass = (float)(density * count);
    float inertia = (float)(density * (second_moment - (sum_x * center_x + sum_y * center_y)));
    uint64_t chunk_x = (uint64_t)initial.chunk.x, chunk_y = (uint64_t)initial.chunk.y;
    dc_gpu_body_record_t record = {.body = body, .visible = 1u,
        .chunk_x = {(uint32_t)chunk_x, (uint32_t)(chunk_x >> 32)},
        .chunk_y = {(uint32_t)chunk_y, (uint32_t)(chunk_y >> 32)},
        .local_x_fp = body.x_fp, .local_y_fp = body.y_fp,
        .reserved = {DC_GPU_STRUCTURAL_CONVERTED_TAG, 1u},
        .shape.material = material, .piece_count = DC_GPU_PIXEL_BODY_FLAG | count,
        .compound_reserved = offset, .motion_ready = 1u,
        .center = {(float)center_x, (float)center_y}, .density = (float)density,
        .mass = mass, .inertia = inertia, .inverse_mass = 1.f / mass,
        .inverse_inertia = 1.f / inertia,
        .radius = (float)hypot(fmax(center_x, body.width - center_x),
                              fmax(center_y, body.height - center_y))};
    record.body.id = id;
    record.body.active = 1u;
    record.body.x_fp = record.previous_x_fp = (int32_t)(grid_x * DC_FLUID_FULL);
    record.body.y_fp = record.previous_y_fp = (int32_t)(grid_y * DC_FLUID_FULL);
    pixel_birth_bounds(&record, grid_x, grid_y);
    memset(dc_gpu_body_solver_state(g, slot), 0, 64u * sizeof(uint32_t));
    *dc_gpu_body_owner(g, slot) = (dc_body_storage_owner_t){
        .generation = generation, .runtime_id = id, .state = DC_BODY_OWNER_LIVE,
        .live_index = slot};
    *dc_gpu_body_record(g, slot) = record;
    g->body_ids[slot] = id;
    if (g->body_count <= slot) g->body_count = slot + 1u;
    g->body_refresh_pending = true;
    *body_id = id;
    return true;
}
