#ifndef DUNGEONCRAFT_CHUNK_H
#define DUNGEONCRAFT_CHUNK_H

#include <stdbool.h>
#include <stdint.h>

#define DC_CHUNK_SIDE 64u
#define DC_CHUNK_CELLS (DC_CHUNK_SIDE * DC_CHUNK_SIDE)
#define DC_FLUID_FULL 65536u

enum {
    DC_MATERIAL_AIR = 0,
    DC_MATERIAL_STONE = 1,
    DC_MATERIAL_SAND = 2
};

typedef struct { int64_t x, y; } dc_chunk_coord_t;

typedef struct {
    uint16_t material;
    uint16_t flags;
    uint32_t fluid_mass;
} dc_cell_t;

typedef struct {
    dc_chunk_coord_t coord;
    dc_cell_t cells[DC_CHUNK_CELLS];
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
