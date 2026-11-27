#ifndef DUNGEONCRAFT_LIQUID_WATER_DOMAIN_GPU_H
#define DUNGEONCRAFT_LIQUID_WATER_DOMAIN_GPU_H

#include "body_address_gpu.h"
#include "liquid_seam_gpu_internal.h"

enum {
    SEAM_TILES = 2,
    OCCUPANCY_PLANES = 5,
    CELL_COVERAGE = 2,
    X_FACE_COVERAGE = 3,
    Y_FACE_COVERAGE = 4,
    WATER_PHASE_BACKUP_FIELDS = 3
};

typedef struct {
    uint32_t sent, received;
} seam_crossing_t;
typedef float seam_color_t[4];

enum {
    CELLS, PAGES, OCCUPANCY, PREVIOUS_VERTICAL, HEAD, VELOCITY, PRESSURE, SLOT_PAGES,
    PRESSURE_WORK, PRESSURE_STENCIL, INITIAL_CELLS, APERTURE_PREFIX, APERTURE_TREE,
    CROSSINGS, WATER_COLOR, INITIAL_COLOR, THERMAL_CONTROL, BULK_PHASE, PUBLICATION_RECEIPT, PUBLICATION_TILES,
    MARKER_A, MARKER_B, COUNT_A, COUNT_B, MARKER_GRID, SLOT_SEED, PREVIOUS_STOCK,
    PROJECTED_VELOCITY, DISPLACEMENT_CROSSINGS, ADVECTION_CROSSINGS, PHASE_BACKUP, BUFFER_COUNT
};

struct dc_gpu_seam_projection {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDescriptorPool pool;
    VkDescriptorSet sets[2];
    VkPipeline transport[4];
    VkPipeline displacement[2];
    VkPipeline correction;
    fr_renderer_compute_t publication;
    dc_gpu_address_buffer_t water_heat;
    dc_gpu_t *phase_peer;
    dc_chunk_coord_t phase_first_coord;
    bool restore_phase;
    uint32_t width, height, tiles;
    VkDeviceSize offset[BUFFER_COUNT], size[BUFFER_COUNT];
    void *mapped;
};

static inline void *projection_field(const struct dc_gpu_seam_projection *scratch, unsigned field) {
    return (unsigned char *)scratch->mapped + scratch->offset[field];
}

/** Resolve a reopened cohort against its new device pages before restoring phase scratch. */
bool dc_gpu_water_checkpoint_layout(const dc_liquid_seam_endpoint_t *tiles, uint32_t count,
    dc_gpu_water_tile_t *locations, uint32_t *width, uint32_t *height);

bool dc_gpu_prepare_water_projection(dc_gpu_t *gpu, uint32_t width, uint32_t height, uint32_t tiles,
    char *err, uint32_t cap);
bool dc_gpu_prepare_water_transport(dc_gpu_t *gpu, bool occupancy, bool thermal, char *err, uint32_t cap);
void dc_gpu_copy_projection_tile(VkCommandBuffer command, const struct dc_gpu_seam_projection *scratch,
    dc_liquid_seam_endpoint_t endpoint, const dc_gpu_water_tile_t *location, uint32_t tile, bool transport,
    bool staged_bulk);

#endif
