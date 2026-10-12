#include <math.h>

#include "structural_store_internal.h"

bool dc_gpu_checkpoint_chunk_valid(const dc_chunk_t *chunk) {
    if (chunk->marker_count > DC_MARKERS_PER_CHUNK ||
        chunk->particle_count > DC_MPM_PARTICLES_PER_CHUNK) return false;
    for (unsigned i = 0; i < DC_CHUNK_CELLS; i++) {
        if (chunk->cells[i].material > DC_MATERIAL_MORTAR_DEBRIS ||
            !isfinite(chunk->face_velocity[i].x) ||
            !isfinite(chunk->face_velocity[i].y)) return false;
    }
    unsigned count = 0;
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
        const dc_mpm_particle_t *p = chunk->particles + i;
        if (!p->mass_fp) continue;
        count++;
        if (p->material != DC_MATERIAL_SAND && p->material != DC_MATERIAL_DIRT &&
            p->material != DC_MATERIAL_GRAVEL && p->material != DC_MATERIAL_MORTAR_DEBRIS)
            return false;
        if (!p->grain_fp) return false;
        for (unsigned j = 0; j < 4; j++)
            if (!isfinite(p->deformation[j])) return false;
    }
    return count == chunk->particle_count;
}
