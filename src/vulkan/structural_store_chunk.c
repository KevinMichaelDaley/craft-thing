#include <math.h>

#include "structural_store_internal.h"
#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "reactive_checkpoint_gpu.h"
#include "structural_store_fluids.h"
#include "thermal_gpu_internal.h"

static bool matching_stream_geometry(const dc_chunk_t *saved, const dc_chunk_t *streamed,
                                     dc_chunk_time_t saved_time, dc_chunk_time_t stream_time) {
    if ((saved_time.known && stream_time.known && saved_time.seconds != stream_time.seconds) ||
        saved->marker_count != streamed->marker_count ||
        saved->particle_count != streamed->particle_count ||
        memcmp(saved->markers, streamed->markers, saved->marker_count * sizeof(*saved->markers)) ||
        memcmp(saved->face_velocity, streamed->face_velocity, sizeof(saved->face_velocity))) return false;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        const dc_cell_t *a = saved->cells + i, *b = streamed->cells + i;
        if (a->material != b->material || a->fluid_mass != b->fluid_mass ||
            ((a->flags ^ b->flags) & ~DC_CELL_LOADED_TERRAIN)) return false;
    }
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i) {
        const dc_mpm_particle_t *a = saved->particles + i, *b = streamed->particles + i;
        if ((a->mass_fp || b->mass_fp) && memcmp(a, b, sizeof(*a))) return false;
    }
    return true;
}

static bool import_chunk_fluids(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
    const fr_gas_checkpoint_chunk_t *gas, const fr_bulk_phase_chunk_t *bulk,
    bool ambient, bool *gas_admitted, char *err, uint32_t cap) {
    static const fr_gas_checkpoint_chunk_t empty_gas;
    if (bulk && !fr_bulk_phase_gpu_import_chunk(gpu, slot, chunk, bulk, err, cap)) return false;
    if (gas) {
        if (!fr_gas_gpu_import_checkpoint(gpu, slot, gas, err, cap)) return false;
    } else if (ambient) {
        if (!fr_gas_gpu_queue_ambient_chunk(gpu, slot, err, cap)) return false;
    } else if (gpu->gas && !fr_gas_gpu_import_checkpoint(gpu, slot, &empty_gas, err, cap)) {
        return false;
    }
    if (gas_admitted) *gas_admitted = gas != NULL || ambient;
    return true;
}

uint32_t dc_gpu_pending_chunk_index(const dc_gpu_t *gpu, dc_chunk_coord_t coord) {
    for (uint32_t i = 0; i < gpu->checkpoint_chunk_count; ++i) {
        dc_chunk_coord_t saved = gpu->checkpoint_chunks[i].coord;
        if (saved.x == coord.x && saved.y == coord.y) return i;
    }
    return UINT32_MAX;
}

static void remove_pending(dc_gpu_t *gpu, uint32_t index) {
    uint32_t last = gpu->checkpoint_chunk_count - 1u;
    if (gpu->checkpoint_times) gpu->checkpoint_times[index] = gpu->checkpoint_times[last];
    if (gpu->checkpoint_owners) gpu->checkpoint_owners[index] = gpu->checkpoint_owners[last];
    if (gpu->checkpoint_fluids) {
        dc_checkpoint_fluid_release(gpu->checkpoint_fluids + index);
        if (index != last) {
            gpu->checkpoint_fluids[index] = gpu->checkpoint_fluids[last];
            gpu->checkpoint_fluids[last] = (dc_checkpoint_fluid_t){0};
        }
    }
    fr_gpu_reactive_checkpoint_remove_pending(gpu, index, last);
    fr_thermal_checkpoint_remove_pending(gpu, index, last);
    gpu->checkpoint_chunks[index] = gpu->checkpoint_chunks[last];
    if (gpu->checkpoint_colors) gpu->checkpoint_colors[index] = gpu->checkpoint_colors[last];
    gpu->checkpoint_chunk_count = last;
}

static bool upload_streamed(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk, bool seed,
    const dc_gpu_stream_channels_t *stream, uint32_t pending, bool *gas_admitted,
    char *err, uint32_t cap) {
    return dc_gpu_upload_chunk_exact(gpu, slot, chunk, seed, err, cap) &&
        (!stream || (import_chunk_fluids(gpu, slot, chunk, stream->gas, stream->bulk,
            stream->ambient_gas, gas_admitted, err, cap) &&
            (!stream->colors || fr_renderer_upload_chunk_colors(gpu, slot, stream->colors, err, cap)) &&
            (!stream->water_excess || fr_thermal_gpu_import_water_chunk(gpu, slot, chunk,
                stream->water_excess, err, cap)) &&
            (pending == UINT32_MAX || fr_gpu_reactive_checkpoint_retire_handles(gpu, pending, err, cap)) &&
            fr_gpu_reactive_import_chunk(gpu, slot, chunk, stream->reactive, err, cap)));
}

static bool upload_saved(dc_gpu_t *gpu, uint32_t slot, uint32_t index,
    const dc_chunk_t *streamed, dc_chunk_time_t saved_time, const dc_gpu_stream_channels_t *stream,
    bool *gas_admitted, char *err, uint32_t cap) {
    const dc_chunk_t *saved = gpu->checkpoint_chunks + index;
    const dc_checkpoint_fluid_t *fluid = gpu->checkpoint_fluids ?
        gpu->checkpoint_fluids + index : NULL;
    const bool own_fluids = fluid && fluid->known;
    const bool matching = !own_fluids && stream && (stream->gas || stream->bulk) &&
        matching_stream_geometry(saved, streamed, saved_time, stream->time);
    const fr_gas_checkpoint_chunk_t *gas = own_fluids ? fluid->gas : (matching ? stream->gas : NULL);
    const fr_bulk_phase_chunk_t *bulk = own_fluids ? fluid->bulk : (matching ? stream->bulk : NULL);
    if (!fr_gpu_reactive_checkpoint_preflight_chunk(gpu, saved, index, err, cap) ||
        !fr_thermal_checkpoint_preflight_chunk(gpu, index, err, cap) ||
        !dc_gpu_upload_chunk_exact(gpu, slot, saved, false, err, cap) ||
        ((own_fluids || stream) && !import_chunk_fluids(gpu, slot, saved, gas, bulk,
            stream && stream->ambient_gas, gas_admitted, err, cap)) ||
        !fr_gpu_reactive_checkpoint_bind_chunk(gpu, slot, index, err, cap) ||
        !fr_thermal_checkpoint_bind_chunk(gpu, slot, index, err, cap)) return false;
    if (gpu->checkpoint_colors) dc_gpu_write_chunk_colors(gpu, slot, gpu->checkpoint_colors + index);
    return true;
}

static bool upload_chunk_checkpoint(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk, bool seed,
    const dc_gpu_stream_channels_t *stream, bool *restored, dc_chunk_time_t *time,
    bool *gas_admitted, char *err, uint32_t cap) {
    if (restored) *restored = false;
    if (time) *time = (dc_chunk_time_t){0};
    if (gas_admitted) *gas_admitted = false;
    uint32_t index = gpu && chunk ? dc_gpu_pending_chunk_index(gpu, chunk->coord) : UINT32_MAX;
    if (index == UINT32_MAX)
        return upload_streamed(gpu, slot, chunk, seed, stream, index, gas_admitted, err, cap);
    dc_chunk_time_t saved_time = gpu->checkpoint_times ? gpu->checkpoint_times[index] : (dc_chunk_time_t){0};
    bool replace = stream && gpu->checkpoint_owners && dc_chunk_owner_valid(stream->replaces_owner) &&
        dc_chunk_owner_equal(stream->replaces_owner, gpu->checkpoint_owners[index]) &&
        saved_time.known && stream->time.known && stream->time.seconds >= saved_time.seconds;
    if (replace) {
        if (!upload_streamed(gpu, slot, chunk, seed, stream, index, gas_admitted, err, cap)) return false;
    } else {
        if (!upload_saved(gpu, slot, index, chunk, saved_time, stream, gas_admitted, err, cap)) return false;
        if (restored) *restored = true;
        if (time) *time = saved_time;
    }
    remove_pending(gpu, index);
    return true;
}

bool dc_gpu_upload_chunk_checkpoint(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
    bool seed, bool *restored, dc_chunk_time_t *time, char *err, uint32_t cap) {
    return upload_chunk_checkpoint(gpu, slot, chunk, seed, NULL, restored, time, NULL, err, cap);
}

bool dc_gpu_upload_streamed_chunk(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
    const dc_gpu_stream_channels_t *stream, bool *restored,
    dc_chunk_time_t *time, bool *gas_admitted, char *err, uint32_t cap) {
    return upload_chunk_checkpoint(gpu, slot, chunk, false, stream, restored, time, gas_admitted, err, cap);
}

bool dc_gpu_checkpoint_chunk_valid(const dc_chunk_t *chunk) {
    if (chunk->marker_count > DC_MARKERS_PER_CHUNK ||
        chunk->particle_count > DC_MPM_PARTICLES_PER_CHUNK)
        return false;
    for (unsigned i = 0; i < DC_CHUNK_CELLS; i++) {
        if ((chunk->cells[i].material > DC_MATERIAL_LEAF_DEBRIS&&
             chunk->cells[i].material!=DC_MATERIAL_ASH&&
             chunk->cells[i].material!=DC_MATERIAL_CORROSION_RESIDUE&&
             chunk->cells[i].material!=DC_MATERIAL_GOLD&&
             chunk->cells[i].material!=DC_MATERIAL_MAGIC_INGREDIENT) ||
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
