#include <math.h>

#include "structural_store_internal.h"

bool dc_gpu_checkpoint_chunk_valid(const dc_chunk_t *chunk) {
    if (chunk->marker_count > DC_MARKERS_PER_CHUNK ||
        chunk->particle_count > DC_MPM_PARTICLES_PER_CHUNK)
        return false;
    for (unsigned i = 0; i < DC_CHUNK_CELLS; i++) {
        if ((chunk->cells[i].material > DC_MATERIAL_LEAF_DEBRIS&&chunk->cells[i].material!=DC_MATERIAL_ASH) ||
            !isfinite(chunk->face_velocity[i].x) ||
            !isfinite(chunk->face_velocity[i].y))
            return false;
    }
    unsigned count = 0;
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
        const dc_mpm_particle_t *p = chunk->particles + i;
        if (!p->mass_fp)
            continue;
        count++;
        if (p->material != DC_MATERIAL_SAND &&
            p->material != DC_MATERIAL_DIRT &&
            p->material != DC_MATERIAL_GRAVEL &&
            p->material != DC_MATERIAL_MORTAR_DEBRIS && p->material != DC_MATERIAL_LEAF_DEBRIS && p->material != DC_MATERIAL_ASH)
            return false;
        if (!p->grain_fp)
            return false;
        for (unsigned j = 0; j < 4; j++)
            if (!isfinite(p->deformation[j]))
                return false;
    }
    return count == chunk->particle_count;
}

bool dc_gpu_checkpoint_write_chunks(FILE *file, const dc_chunk_t *chunks,
                                    unsigned count) {
    for (unsigned c = 0; c < count; c++) {
        const dc_chunk_t *chunk = chunks + c;
        uint32_t counts[] = {chunk->marker_count, chunk->particle_count};
        if (fwrite(&chunk->coord, sizeof(chunk->coord), 1, file) != 1 ||
            fwrite(counts, sizeof(counts), 1, file) != 1 ||
            fwrite(chunk->cells, sizeof(chunk->cells), 1, file) != 1 ||
            fwrite(chunk->face_velocity, sizeof(chunk->face_velocity), 1,
                   file) != 1 ||
            fwrite(chunk->markers, sizeof(*chunk->markers), chunk->marker_count,
                   file) != chunk->marker_count)
            return false;
        for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
            if (!chunk->particles[i].mass_fp)
                continue;
            if (fwrite(&i, sizeof(i), 1, file) != 1 ||
                fwrite(chunk->particles + i, sizeof(*chunk->particles), 1,
                       file) != 1)
                return false;
        }
    }
    return true;
}

bool dc_gpu_checkpoint_read_chunks(FILE *file, dc_chunk_t *chunks,
                                   unsigned count) {
    for (unsigned c = 0; c < count; c++) {
        dc_chunk_t *chunk = chunks + c;
        uint32_t counts[2];
        if (fread(&chunk->coord, sizeof(chunk->coord), 1, file) != 1 ||
            fread(counts, sizeof(counts), 1, file) != 1 ||
            counts[0] > DC_MARKERS_PER_CHUNK ||
            counts[1] > DC_MPM_PARTICLES_PER_CHUNK)
            return false;
        chunk->marker_count = counts[0];
        chunk->particle_count = counts[1];
        if (fread(chunk->cells, sizeof(chunk->cells), 1, file) != 1 ||
            fread(chunk->face_velocity, sizeof(chunk->face_velocity), 1,
                  file) != 1 ||
            fread(chunk->markers, sizeof(*chunk->markers), chunk->marker_count,
                  file) != chunk->marker_count)
            return false;
        for (unsigned i = 0; i < chunk->particle_count; i++) {
            uint32_t index;
            if (fread(&index, sizeof(index), 1, file) != 1 ||
                index >= DC_MPM_PARTICLES_PER_CHUNK ||
                chunk->particles[index].mass_fp ||
                fread(chunk->particles + index, sizeof(*chunk->particles), 1,
                      file) != 1 ||
                !chunk->particles[index].mass_fp)
                return false;
        }
    }
    return true;
}
