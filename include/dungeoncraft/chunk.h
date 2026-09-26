#ifndef DUNGEONCRAFT_CHUNK_H
#define DUNGEONCRAFT_CHUNK_H

#include <stdbool.h>
#include <stdint.h>

#define DC_CHUNK_SIDE 64u
#define DC_CHUNK_CELLS (DC_CHUNK_SIDE * DC_CHUNK_SIDE)
#define DC_FLUID_FULL 65536u
#define DC_MARKERS_PER_CHUNK 2048u
#define DC_MPM_PARTICLES_PER_CHUNK 8192u

enum { DC_MARKER_INSIDE = 1u, DC_MARKER_OUTSIDE = 2u };

typedef struct {
    int32_t x_fp, y_fp;
    uint32_t id, kind;
} dc_marker_t;

enum {
    DC_MATERIAL_AIR = 0,
    DC_MATERIAL_STONE = 1,
    DC_MATERIAL_SAND = 2,
    DC_MATERIAL_WATER = 3,
    DC_MATERIAL_DIRT = 4,
    DC_MATERIAL_GRAVEL = 5
};

typedef struct { int64_t x, y; } dc_chunk_coord_t;

typedef struct {
    uint16_t material;
    uint16_t flags;
    uint32_t fluid_mass;
} dc_cell_t;

typedef struct { float x, y; } dc_face_velocity_t;

typedef struct {
    int32_t x_fp, y_fp, vx_fp, vy_fp;
    float deformation[4];
    uint32_t id_lo, id_hi;
    uint32_t mass_fp, grain_fp;
    uint32_t material, flags;
} dc_mpm_particle_t;

typedef struct {
    dc_chunk_coord_t coord;
    dc_cell_t cells[DC_CHUNK_CELLS];
    dc_face_velocity_t face_velocity[DC_CHUNK_CELLS];
    uint32_t marker_count;
    dc_marker_t markers[DC_MARKERS_PER_CHUNK];
    uint32_t particle_count;
    dc_mpm_particle_t particles[DC_MPM_PARTICLES_PER_CHUNK];
} dc_chunk_t;

typedef enum {
    DC_SLOT_EMPTY,
    DC_SLOT_LOADING,
    DC_SLOT_ACTIVE,
    DC_SLOT_SLEEPING,
    DC_SLOT_SAVING
} dc_slot_state_t;

typedef struct {
    dc_chunk_coord_t coord;
    dc_slot_state_t state;
    uint64_t generation;
    uint64_t last_gpu_use;
    bool dirty;
    bool pinned;
} dc_chunk_slot_t;

typedef struct {
    dc_chunk_slot_t *slots;
    uint32_t capacity;
    uint64_t next_generation;
} dc_chunk_table_t;

/** Convert signed world-cell coordinates to a chunk and nonnegative local cell. */
void dc_cell_address(int64_t x, int64_t y, dc_chunk_coord_t *coord,
                     uint32_t *local_x, uint32_t *local_y);

/** Allocate a fixed pool of chunk slots. */
bool dc_chunk_table_init(dc_chunk_table_t *table, uint32_t capacity);

/** Release the slot pool. */
void dc_chunk_table_destroy(dc_chunk_table_t *table);

/** Find a resident or loading slot by world coordinate. */
bool dc_chunk_table_find(const dc_chunk_table_t *table, dc_chunk_coord_t coord,
                         uint32_t *index);

/** Reserve an empty slot for an asynchronous load and return its generation. */
bool dc_chunk_table_begin_load(dc_chunk_table_t *table, dc_chunk_coord_t coord,
                               uint32_t *index, uint64_t *generation);

/** Publish a load only when its generation still owns the slot. */
bool dc_chunk_table_finish_load(dc_chunk_table_t *table, uint32_t index,
                                uint64_t generation);

/** Mark a loaded chunk active or sleeping. */
bool dc_chunk_table_set_active(dc_chunk_table_t *table, uint32_t index, bool active);

/** Mark a chunk modified; dirty chunks require saving before eviction. */
bool dc_chunk_table_mark_dirty(dc_chunk_table_t *table, uint32_t index);

/** Freeze a sleeping dirty slot for an asynchronous save. */
bool dc_chunk_table_begin_save(dc_chunk_table_t *table, uint32_t index,
                               uint64_t *generation);

/** Clear dirty state after the matching asynchronous save completes. */
bool dc_chunk_table_finish_save(dc_chunk_table_t *table, uint32_t index,
                                uint64_t generation);

/** Evict only a clean, unpinned sleeping slot after GPU use has completed. */
bool dc_chunk_table_evict(dc_chunk_table_t *table, uint32_t index,
                          uint64_t completed_gpu_value);

#endif
