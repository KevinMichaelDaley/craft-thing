#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structural_store_internal.h"

_Static_assert(sizeof(dc_structural_store_header_t) == 56,
               "Structure snapshot header ABI");
_Static_assert(sizeof(dc_gpu_body_record_t) == 544,
               "Structure snapshot body ABI");
enum {
    STORE_BODY_WORDS =
        DC_GPU_BODY_CAPACITY * sizeof(dc_gpu_body_record_t) / sizeof(uint32_t)
};

bool fr_gpu_upload_structure_chunk(dc_gpu_t *gpu, uint32_t slot,
                                   const dc_chunk_t *chunk, bool *restored,
                                   char *err, uint32_t cap) {
    return dc_gpu_upload_chunk_checkpoint(gpu, slot, chunk, false, restored,
                                          err, cap);
}

bool fr_gpu_structure_checkpoint_required(const dc_gpu_t *gpu) {
    return gpu && gpu->structural_enabled;
}

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap)
        snprintf(err, cap, "%s", message);
    return false;
}

static uint64_t hash_bytes(uint64_t hash, const void *bytes, size_t count) {
    const unsigned char *p = bytes;
    for (size_t i = 0; i < count; i++)
        hash = (hash ^ p[i]) * UINT64_C(1099511628211);
    return hash;
}

static uint64_t checksum(dc_structural_store_header_t header,
                         const dc_structural_store_payload_t *payload,
                         const uint32_t *pixels, const dc_chunk_t *chunks) {
    header.checksum = 0;
    uint64_t hash =
        hash_bytes(UINT64_C(14695981039346656037), &header, sizeof(header));
    hash = hash_bytes(hash, payload, sizeof(*payload));
    hash =
        hash_bytes(hash, pixels, (size_t)header.reserved[1] * sizeof(*pixels));
    return hash_bytes(hash, chunks,
                      (size_t)header.reserved[0] * sizeof(*chunks));
}

static void clear_transients(uint32_t *graph) {
    memset(graph + DC_STRUCTURAL_GRAPH_WORDS, 0,
           (24000u - DC_STRUCTURAL_GRAPH_WORDS) * sizeof(uint32_t));
    for (unsigned i = 0; i < 32; i++)
        if (graph[24400u + i * 16u + 1u] != FR_STRUCTURAL_CONVERTED)
            memset(graph + 24400u + i * 16u, 0, 16 * sizeof(uint32_t));
    memset(graph + 24912u, 0, (28000u - 24912u) * sizeof(uint32_t));
    memset(graph + 28128u, 0, (28300u - 28128u) * sizeof(uint32_t));
    memset(graph + 34000u, 0, 1024u * sizeof(uint32_t));
    memset(graph + 39000u, 0, 1000u * sizeof(uint32_t));
    for (unsigned i = 0; i < 192; i++)
        memset(graph + 30000u + i * 20u + 15u, 0, 3 * sizeof(uint32_t));
}

bool fr_gpu_save_structure(dc_gpu_t *gpu, const char *path, char *err,
                           uint32_t cap) {
    if (!gpu || !path || !*path)
        return error(err, cap, "Invalid structure snapshot path");
    if (!gpu->structural_enabled)
        return dc_gpu_save_bodies(gpu, path, err, cap);
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "Cannot reach snapshot boundary");
    char temporary[1024];
    int length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return error(err, cap, "Snapshot path too long");
    dc_structural_store_payload_t *payload = calloc(1, sizeof(*payload));
    if (!payload)
        return error(err, cap, "Cannot allocate snapshot staging");
    uint32_t *data = dc_gpu_structural_data(gpu);
    payload->control[0] = data[0];
    payload->control[1] = data[3];
    payload->control[2] = data[6];
    payload->control[3] = data[14];
    memcpy(payload->bodies, gpu->body_mapped, sizeof(payload->bodies));
    memcpy(payload->graph, data + dc_gpu_structural_graph_offset(),
           sizeof(payload->graph));
    clear_transients(payload->graph);
    dc_structural_store_header_t header = {
        .magic = {'D', 'C', 'S', '1'},
        .version = 4,
        .body_words = STORE_BODY_WORDS,
        .graph_words = DC_STRUCTURAL_RESERVE_WORDS,
        .flags = 1u | (gpu->impact_fracture_enabled ? 2u : 0u) |
                 (gpu->automatic_components_enabled ? 4u : 0u),
        .origin = gpu->body_origin};
    uint32_t capacity = dc_gpu_pixel_pool_capacity(gpu);
    uint32_t *pixels = calloc(capacity, sizeof(*pixels));
    bool okay = pixels != NULL;
    for (unsigned i = 0; okay && i < DC_GPU_BODY_CAPACITY; i++) {
        dc_gpu_body_record_t *b = payload->bodies + i;
        if ((!b->body.active && !b->reserved[0]) ||
            !(b->piece_count & DC_GPU_PIXEL_BODY_FLAG))
            continue;
        uint64_t words = ((uint64_t)b->body.width * b->body.height + 7u) / 8u;
        okay = b->compound_reserved >= dc_gpu_pixel_pool_word_offset(gpu) +
                                           DC_GPU_PIXEL_POOL_HEADER_WORDS &&
               b->compound_reserved + words <=
                   dc_gpu_body_storage_bytes(gpu) / 4 &&
               words <= capacity - header.reserved[1];
        if (!okay)
            break;
        memcpy(pixels + header.reserved[1],
               (uint32_t *)gpu->body_mapped + b->compound_reserved,
               words * sizeof(*pixels));
        b->compound_reserved = header.reserved[1];
        header.reserved[1] += words;
    }
    okay = okay && dc_gpu_structure_payload_valid(gpu, payload, pixels,
                                                  header.reserved[1]);
    dc_chunk_t *chunks = calloc(
        DC_GPU_CHUNK_SLOTS + gpu->checkpoint_chunk_count, sizeof(*chunks));
    okay = okay && chunks != NULL;
    if (okay && gpu->checkpoint_chunk_count) {
        memcpy(chunks, gpu->checkpoint_chunks,
               (size_t)gpu->checkpoint_chunk_count * sizeof(*chunks));
        header.reserved[0] = gpu->checkpoint_chunk_count;
    }
    for (unsigned slot = 0; okay && slot < DC_GPU_CHUNK_SLOTS; slot++) {
        const uint32_t *record = data + DC_STRUCTURAL_HEADER_WORDS + slot * 8u;
        if (!record[5])
            continue;
        dc_chunk_t *chunk = chunks + header.reserved[0];
        memcpy(&chunk->coord, record, sizeof(chunk->coord));
        okay = dc_gpu_download_chunk(gpu, slot, chunk, err, cap) &&
               dc_gpu_checkpoint_chunk_valid(chunk);
        if (okay)
            header.reserved[0]++;
    }
    for (unsigned c = 0; okay && c < header.reserved[0]; c++) {
        dc_chunk_t *chunk = chunks + c;
        memset(chunk->markers + chunk->marker_count, 0,
               (DC_MARKERS_PER_CHUNK - chunk->marker_count) *
                   sizeof(*chunk->markers));
        for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++)
            if (!chunk->particles[i].mass_fp)
                memset(chunk->particles + i, 0, sizeof(*chunk->particles));
    }
    okay = okay && header.reserved[0] <= DC_GPU_CHUNK_SLOTS * 2u;
    header.checksum = checksum(header, payload, pixels, chunks);
    FILE *file = okay ? fopen(temporary, "wb") : NULL;
    if (file) {
        okay = fwrite(&header, sizeof(header), 1, file) == 1 &&
               fwrite(payload, sizeof(*payload), 1, file) == 1 &&
               fwrite(pixels, sizeof(*pixels), header.reserved[1], file) ==
                   header.reserved[1] &&
               dc_gpu_checkpoint_write_chunks(file, chunks, header.reserved[0]);
        if (fclose(file) != 0)
            okay = false;
        if (okay)
            okay = rename(temporary, path) == 0;
        if (!okay)
            remove(temporary);
    } else
        okay = false;
    free(payload);
    free(pixels);
    free(chunks);
    return okay || error(err, cap, "Cannot write validated structure snapshot");
}

static bool refresh_topology(dc_gpu_t *gpu) {
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return false;
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return false;
    dc_gpu_record_structural_topology(gpu);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return false;
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &gpu->command};
    return vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) ==
               VK_SUCCESS &&
           vkQueueWaitIdle(gpu->queue) == VK_SUCCESS;
}

static void install(dc_gpu_t *gpu, const dc_structural_store_header_t *header,
                    const dc_structural_store_payload_t *payload,
                    const uint32_t *pixels) {
    uint32_t *data = dc_gpu_structural_data(gpu);
    memcpy(gpu->body_mapped, payload->bodies, sizeof(payload->bodies));
    uint32_t offset = dc_gpu_pixel_pool_word_offset(gpu);
    uint32_t *pool = (uint32_t *)gpu->body_mapped + offset;
    memset(pool, 0, DC_GPU_PIXEL_POOL_HEADER_WORDS * sizeof(*pool));
    pool[1] = header->reserved[1];
    pool[2] = dc_gpu_pixel_pool_capacity(gpu);
    memcpy(pool + DC_GPU_PIXEL_POOL_HEADER_WORDS, pixels,
           (size_t)header->reserved[1] * sizeof(*pixels));
    for (unsigned i = 0; i < DC_GPU_BODY_CAPACITY; i++) {
        dc_gpu_body_record_t *b = (dc_gpu_body_record_t *)gpu->body_mapped + i;
        if (!b->body.active && !b->reserved[0])
            memset(b, 0, sizeof(*b));
        else if (b->piece_count & DC_GPU_PIXEL_BODY_FLAG)
            b->compound_reserved += offset + DC_GPU_PIXEL_POOL_HEADER_WORDS;
    }
    memcpy(data + dc_gpu_structural_graph_offset(), payload->graph,
           sizeof(payload->graph));
    data[0] = payload->control[0];
    data[3] = payload->control[1] + 1u;
    data[4] = data[3] ^ 1u;
    data[6] = payload->control[2];
    data[14] = payload->control[3];
    data[9] = 0;
    data[10] = 1;
    data[11] = 1;
    for (unsigned i = 0; i < 192; i++)
        data[9] |=
            (data[dc_gpu_structural_graph_offset() + 30000u + i * 20u] != 0);
    data[15] = (header->flags & 2u) != 0;
    data[19] = (header->flags & 4u) != 0;
    data[DC_STRUCTURAL_COMPONENT_DIRTY_WORD] = data[19];
    data[16] = 0;
    data[17] = (gpu->height + 15u) / 16u;
    data[18] = 1;
    gpu->structural_enabled = true;
    gpu->impact_fracture_enabled = (header->flags & 2u) != 0;
    gpu->automatic_components_enabled = (header->flags & 4u) != 0;
    gpu->body_origin = header->origin;
    gpu->body_refresh_pending = true;
    dc_gpu_refresh_body_registry(gpu);
}

bool fr_gpu_load_structure(dc_gpu_t *gpu, const char *path, char *err,
                           uint32_t cap) {
    if (!gpu || !path || !*path)
        return error(err, cap, "Invalid structure snapshot path");
    FILE *file = fopen(path, "rb");
    if (!file)
        return errno == ENOENT ||
               error(err, cap, "Cannot open structure snapshot");
    char magic[4];
    bool framed = fread(magic, sizeof(magic), 1, file) == 1;
    if (framed && memcmp(magic, "DCB1", 4) == 0) {
        fclose(file);
        return dc_gpu_load_bodies(gpu, path, err, cap);
    }
    rewind(file);
    dc_structural_store_header_t header = {0};
    dc_structural_store_payload_t *payload = calloc(1, sizeof(*payload));
    bool okay =
        payload && framed && fread(&header, sizeof(header), 1, file) == 1 &&
        memcmp(header.magic, "DCS1", 4) == 0 && header.version >= 1 &&
        header.version <= 4 && header.body_words == STORE_BODY_WORDS &&
        header.graph_words == DC_STRUCTURAL_RESERVE_WORDS &&
        (header.flags & 1u) && !(header.flags & ~7u) &&
        header.reserved[0] <= DC_GPU_CHUNK_SLOTS * 2u &&
        (header.version >= 2 || !header.reserved[0]) &&
        (header.version >= 4 || !header.reserved[1]) &&
        header.reserved[1] <= dc_gpu_pixel_pool_capacity(gpu) &&
        !header.reserved[2] && fread(payload, sizeof(*payload), 1, file) == 1;
    uint32_t *pixels =
        okay ? calloc(header.reserved[1] + 1u, sizeof(*pixels)) : NULL;
    okay = okay && pixels &&
           fread(pixels, sizeof(*pixels), header.reserved[1], file) ==
               header.reserved[1];
    dc_chunk_t *chunks =
        okay ? calloc(header.reserved[0] + 1u, sizeof(*chunks)) : NULL;
    okay =
        okay && chunks &&
        (header.version >= 3
             ? dc_gpu_checkpoint_read_chunks(file, chunks, header.reserved[0])
             : fread(chunks, sizeof(*chunks), header.reserved[0], file) ==
                   header.reserved[0]) &&
        fgetc(file) == EOF && !ferror(file);
    for (unsigned i = 0; okay && i < header.reserved[0]; i++) {
        okay = dc_gpu_checkpoint_chunk_valid(chunks + i);
        for (unsigned j = 0; okay && j < i; j++)
            okay = chunks[i].coord.x != chunks[j].coord.x ||
                   chunks[i].coord.y != chunks[j].coord.y;
    }
    if (fclose(file) != 0)
        okay = false;
    okay = okay &&
           header.checksum == checksum(header, payload, pixels, chunks) &&
           dc_gpu_structure_payload_valid(gpu, payload, pixels,
                                          header.reserved[1]);
    if (!okay) {
        free(payload);
        free(pixels);
        free(chunks);
        return error(err, cap, "Invalid or truncated structure snapshot");
    }
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS) {
        free(payload);
        free(pixels);
        free(chunks);
        return error(err, cap, "Cannot reach restore boundary");
    }
    dc_chunk_coord_t origin = gpu->body_origin;
    install(gpu, &header, payload, pixels);
    free(payload);
    free(pixels);
    free(gpu->checkpoint_chunks);
    gpu->checkpoint_chunks = chunks;
    gpu->checkpoint_chunk_count = header.reserved[0];
    for (unsigned slot = 0; okay && slot < DC_GPU_CHUNK_SLOTS; slot++) {
        const uint32_t *record = dc_gpu_structural_data(gpu) +
                                 DC_STRUCTURAL_HEADER_WORDS + slot * 8u;
        if (!record[5])
            continue;
        dc_chunk_coord_t coord;
        memcpy(&coord, record, sizeof(coord));
        for (unsigned i = 0; i < gpu->checkpoint_chunk_count; i++) {
            dc_chunk_t *chunk = gpu->checkpoint_chunks + i;
            if (chunk->coord.x != coord.x || chunk->coord.y != coord.y)
                continue;
            okay = dc_gpu_upload_chunk_exact(gpu, slot, chunk, false, err, cap);
            if (okay)
                *chunk = gpu->checkpoint_chunks[--gpu->checkpoint_chunk_count];
            break;
        }
    }
    okay = okay && dc_gpu_set_rigid_solver(gpu, true) &&
           dc_gpu_set_body_origin(gpu, origin, err, cap) &&
           refresh_topology(gpu);
    return okay || error(err, cap, "Cannot restore structural GPU state");
}
