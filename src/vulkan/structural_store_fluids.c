#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../fire/bulk_phase_internal.h"
#include "../fire/gas_checkpoint_internal.h"
#include "ferrum/fire/gas_gpu.h"
#include "gpu_internal.h"
#include "structural_store_fluids.h"

enum { FLUIDS_KNOWN = 1u, FLUIDS_GAS = 2u, FLUIDS_BULK = 4u };

_Static_assert(sizeof(fr_bulk_phase_chunk_t) ==
    FR_BULK_CHUNK_CHANNELS * DC_CHUNK_CELLS * sizeof(uint32_t),
    "Structure bulk checkpoint layout");
_Static_assert(sizeof(fr_gas_checkpoint_chunk_t) == 311320u,
    "Structure gas checkpoint layout");

size_t dc_checkpoint_fluid_max_bytes(uint32_t count) {
    return (size_t)count * (1u + sizeof(fr_gas_checkpoint_chunk_t) + sizeof(fr_bulk_phase_chunk_t));
}

void dc_checkpoint_fluid_release(dc_checkpoint_fluid_t *record) {
    free(record->gas);
    free(record->bulk);
    *record = (dc_checkpoint_fluid_t){0};
}

void dc_checkpoint_fluid_destroy(dc_checkpoint_fluid_t *records, uint32_t count) {
    if (!records) return;
    for (uint32_t i = 0; i < count; ++i) dc_checkpoint_fluid_release(records + i);
    free(records);
}

static bool bulk_has_stock(const fr_bulk_phase_chunk_t *bulk) {
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        if (bulk->oil_fp[i] || bulk->slime_fp[i]) return true;
    }
    return false;
}

bool dc_checkpoint_fluid_encode(dc_gpu_t *gpu, const dc_chunk_t *chunks,
    const uint32_t *slots, uint32_t count, const bool *gas_admitted,
    uint8_t **out, size_t *length, char *err, uint32_t cap) {
    if (!count || (!gas_admitted && !gpu->checkpoint_fluids)) return true;
    uint8_t *bytes = malloc(dc_checkpoint_fluid_max_bytes(count));
    fr_gas_checkpoint_chunk_t *scratch = NULL;
    if (!bytes) return false;
    memset(bytes, 0, count);
    size_t used = count;
    bool any_known = false;
    for (uint32_t i = 0; i < count; ++i) {
        const fr_gas_checkpoint_chunk_t *gas = NULL;
        const fr_bulk_phase_chunk_t *bulk = NULL;
        uint32_t slot = slots[i];
        if (slot == UINT32_MAX) {
            if (!gpu->checkpoint_fluids || !gpu->checkpoint_fluids[i].known) continue;
            gas = gpu->checkpoint_fluids[i].gas;
            bulk = gpu->checkpoint_fluids[i].bulk;
        } else {
            if (!gas_admitted) continue;
            bulk = (const fr_bulk_phase_chunk_t *)gpu->bulk_phase_mapped + slot;
            if (!bulk_phase_chunk_valid(chunks + i, bulk)) goto failed;
            if (!bulk_has_stock(bulk)) bulk = NULL;
            if (gas_admitted[slot]) {
                if (!scratch) scratch = malloc(sizeof(*scratch));
                if (!scratch || !fr_gas_gpu_export_checkpoint(gpu, slot, scratch, err, cap)) goto failed;
                gas = scratch;
            }
        }
        any_known = true;
        bytes[i] = FLUIDS_KNOWN;
        if (gas) {
            if (!gas_checkpoint_valid(gas)) goto failed;
            bytes[i] |= FLUIDS_GAS;
            memcpy(bytes + used, gas, sizeof(*gas));
            used += sizeof(*gas);
        }
        if (bulk) {
            bytes[i] |= FLUIDS_BULK;
            memcpy(bytes + used, bulk, sizeof(*bulk));
            used += sizeof(*bulk);
        }
    }
    free(scratch);
    if (!any_known) {
        free(bytes);
        return true;
    }
    *out = bytes;
    *length = used;
    return true;
failed:
    free(scratch);
    free(bytes);
    if (err && cap && !err[0]) snprintf(err, cap, "Cannot checkpoint authoritative chunk fluids");
    return false;
}

bool dc_checkpoint_fluid_decode(const dc_chunk_t *chunks, uint32_t count,
    const uint8_t *bytes, size_t length, dc_checkpoint_fluid_t **out) {
    if (!chunks || !bytes || !out || !count || length < count ||
        length > dc_checkpoint_fluid_max_bytes(count)) return false;
    dc_checkpoint_fluid_t *records = calloc(count, sizeof(*records));
    if (!records) return false;
    size_t used = count;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t flags = bytes[i];
        if (!flags) continue;
        if (!(flags & FLUIDS_KNOWN) || (flags & ~(FLUIDS_KNOWN | FLUIDS_GAS | FLUIDS_BULK))) goto failed;
        dc_checkpoint_fluid_t *record = records + i;
        record->known = true;
        if (flags & FLUIDS_GAS) {
            if (length - used < sizeof(*record->gas)) goto failed;
            record->gas = malloc(sizeof(*record->gas));
            if (!record->gas) goto failed;
            memcpy(record->gas, bytes + used, sizeof(*record->gas));
            used += sizeof(*record->gas);
            if (!gas_checkpoint_valid(record->gas)) goto failed;
        }
        if (flags & FLUIDS_BULK) {
            if (length - used < sizeof(*record->bulk)) goto failed;
            record->bulk = malloc(sizeof(*record->bulk));
            if (!record->bulk) goto failed;
            memcpy(record->bulk, bytes + used, sizeof(*record->bulk));
            used += sizeof(*record->bulk);
            if (!bulk_phase_chunk_valid(chunks + i, record->bulk)) goto failed;
        }
    }
    if (used != length) goto failed;
    *out = records;
    return true;
failed:
    dc_checkpoint_fluid_destroy(records, count);
    return false;
}
