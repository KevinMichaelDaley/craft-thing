#ifndef DUNGEONCRAFT_STRUCTURAL_STORE_INTERNAL_H
#define DUNGEONCRAFT_STRUCTURAL_STORE_INTERNAL_H

#include "gpu_internal.h"

typedef struct {
    char magic[4];
    uint32_t version, body_words, graph_words, flags, reserved[3];
    uint64_t checksum;
    dc_chunk_coord_t origin;
} dc_structural_store_header_t;

typedef struct {
    uint32_t control[8];
    dc_gpu_body_record_t bodies[DC_GPU_BODY_CAPACITY];
    uint32_t graph[DC_STRUCTURAL_RESERVE_WORDS];
} dc_structural_store_payload_t;

/** Validate bounds, material geometry, ownership and authoritative state. */
bool dc_gpu_structure_payload_valid(const dc_gpu_t *gpu,
                                    const dc_structural_store_payload_t *payload);
bool dc_gpu_checkpoint_chunk_valid(const dc_chunk_t *chunk);

#endif
