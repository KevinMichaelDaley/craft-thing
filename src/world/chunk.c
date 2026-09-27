#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/chunk.h"

_Static_assert(sizeof(dc_cell_t) == 8, "Cell layout must match the GPU ABI");
_Static_assert(sizeof(dc_mpm_particle_t) == 56, "Particle layout must match the GPU ABI");

uint32_t dc_chunk_particle_seed(dc_chunk_coord_t coord) {
    uint64_t key = (uint64_t)coord.x * UINT64_C(0x9e3779b97f4a7c15) ^
                   (uint64_t)coord.y * UINT64_C(0xbf58476d1ce4e5b9);
    key ^= key >> 30;
    key *= UINT64_C(0xbf58476d1ce4e5b9);
    key ^= key >> 27;
    return (uint32_t)(key ^ (key >> 32));
}

uint32_t dc_chunk_grain_radius_fp(uint32_t material) {
    return material == DC_MATERIAL_SAND ? DC_SAND_GRAIN_RADIUS_FP :
           material == DC_MATERIAL_DIRT ? DC_DIRT_GRAIN_RADIUS_FP :
           DC_GRAVEL_GRAIN_RADIUS_FP;
}

void dc_chunk_seed_particles(dc_chunk_t *chunk) {
    if (!chunk) return;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        uint32_t material = chunk->cells[i].material;
        if ((material != DC_MATERIAL_SAND && material != DC_MATERIAL_DIRT &&
             material != DC_MATERIAL_GRAVEL) || chunk->particles[i].mass_fp) continue;
        dc_chunk_particle_init(&chunk->particles[i], chunk->coord, i, material);
        ++chunk->particle_count;
    }
}

void dc_chunk_particle_init(dc_mpm_particle_t *particle,
                            dc_chunk_coord_t coord, uint32_t cell_index,
                            uint32_t material) {
    memset(particle, 0, sizeof(*particle));
    particle->x_fp = (int32_t)((cell_index % DC_CHUNK_SIDE) * DC_FLUID_FULL + DC_FLUID_FULL / 2);
    particle->y_fp = (int32_t)((cell_index / DC_CHUNK_SIDE) * DC_FLUID_FULL + DC_FLUID_FULL / 2);
    particle->deformation[0] = particle->deformation[3] = 1.0f;
    particle->id_lo = cell_index + 1u;
    particle->id_hi = dc_chunk_particle_seed(coord);
    particle->mass_fp = DC_FLUID_FULL;
    particle->grain_fp = dc_chunk_grain_radius_fp(material);
    particle->material = material;
}

static int64_t floor_chunk(int64_t cell, uint32_t *local) {
    int64_t quotient = cell / (int64_t)DC_CHUNK_SIDE;
    int64_t remainder = cell % (int64_t)DC_CHUNK_SIDE;
    if (remainder < 0) {
        --quotient;
        remainder += DC_CHUNK_SIDE;
    }
    *local = (uint32_t)remainder;
    return quotient;
}

void dc_cell_address(int64_t x, int64_t y, dc_chunk_coord_t *coord,
                     uint32_t *local_x, uint32_t *local_y) {
    coord->x = floor_chunk(x, local_x);
    coord->y = floor_chunk(y, local_y);
}

bool dc_chunk_table_init(dc_chunk_table_t *table, uint32_t capacity) {
    if (!table || !capacity) return false;
    memset(table, 0, sizeof(*table));
    table->slots = calloc(capacity, sizeof(*table->slots));
    if (!table->slots) return false;
    table->capacity = capacity;
    table->next_generation = 1;
    return true;
}

void dc_chunk_table_destroy(dc_chunk_table_t *table) {
    if (!table) return;
    free(table->slots);
    memset(table, 0, sizeof(*table));
}

bool dc_chunk_table_find(const dc_chunk_table_t *table, dc_chunk_coord_t coord,
                         uint32_t *index) {
    if (!table || !table->slots) return false;
    for (uint32_t i = 0; i < table->capacity; ++i) {
        const dc_chunk_slot_t *slot = &table->slots[i];
        if (slot->state != DC_SLOT_EMPTY && slot->coord.x == coord.x &&
            slot->coord.y == coord.y) {
            if (index) *index = i;
            return true;
        }
    }
    return false;
}

bool dc_chunk_table_begin_load(dc_chunk_table_t *table, dc_chunk_coord_t coord,
                               uint32_t *index, uint64_t *generation) {
    if (!table || !table->slots || !index || !generation ||
        dc_chunk_table_find(table, coord, NULL)) return false;
    for (uint32_t i = 0; i < table->capacity; ++i) {
        dc_chunk_slot_t *slot = &table->slots[i];
        if (slot->state != DC_SLOT_EMPTY) continue;
        if (table->next_generation == UINT64_MAX) return false;
        memset(slot, 0, sizeof(*slot));
        slot->coord = coord;
        slot->state = DC_SLOT_LOADING;
        slot->generation = table->next_generation++;
        *index = i;
        *generation = slot->generation;
        return true;
    }
    return false;
}

bool dc_chunk_table_finish_load(dc_chunk_table_t *table, uint32_t index,
                                uint64_t generation) {
    if (!table || index >= table->capacity) return false;
    dc_chunk_slot_t *slot = &table->slots[index];
    if (slot->state != DC_SLOT_LOADING || slot->generation != generation) return false;
    slot->state = DC_SLOT_ACTIVE;
    return true;
}

bool dc_chunk_table_set_active(dc_chunk_table_t *table, uint32_t index, bool active) {
    if (!table || index >= table->capacity) return false;
    dc_chunk_slot_t *slot = &table->slots[index];
    if (slot->state != DC_SLOT_ACTIVE && slot->state != DC_SLOT_SLEEPING) return false;
    slot->state = active ? DC_SLOT_ACTIVE : DC_SLOT_SLEEPING;
    return true;
}

bool dc_chunk_table_mark_dirty(dc_chunk_table_t *table, uint32_t index) {
    if (!table || index >= table->capacity) return false;
    dc_chunk_slot_t *slot = &table->slots[index];
    if (slot->state != DC_SLOT_ACTIVE && slot->state != DC_SLOT_SLEEPING) return false;
    slot->dirty = true;
    return true;
}

bool dc_chunk_table_begin_save(dc_chunk_table_t *table, uint32_t index,
                               uint64_t *generation) {
    if (!table || index >= table->capacity || !generation) return false;
    dc_chunk_slot_t *slot = &table->slots[index];
    if (slot->state != DC_SLOT_SLEEPING || !slot->dirty || slot->pinned) return false;
    slot->state = DC_SLOT_SAVING;
    *generation = slot->generation;
    return true;
}

bool dc_chunk_table_finish_save(dc_chunk_table_t *table, uint32_t index,
                                uint64_t generation) {
    if (!table || index >= table->capacity) return false;
    dc_chunk_slot_t *slot = &table->slots[index];
    if (slot->state != DC_SLOT_SAVING || slot->generation != generation) return false;
    slot->dirty = false;
    slot->state = DC_SLOT_SLEEPING;
    return true;
}

bool dc_chunk_table_evict(dc_chunk_table_t *table, uint32_t index,
                          uint64_t completed_gpu_value) {
    if (!table || index >= table->capacity) return false;
    dc_chunk_slot_t *slot = &table->slots[index];
    if (slot->state != DC_SLOT_SLEEPING || slot->dirty || slot->pinned ||
        slot->last_gpu_use > completed_gpu_value) return false;
    memset(slot, 0, sizeof(*slot));
    return true;
}
