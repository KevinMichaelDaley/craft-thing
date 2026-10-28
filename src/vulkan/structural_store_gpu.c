#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structural_store_internal.h"
#include "structural_store_channels.h"
#include "runtime_body_internal.h"

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
                         const uint32_t *pixels, const dc_chunk_t *chunks,
                         const float *body_colors,const dc_chunk_colors_t *colors,
                         const dc_checkpoint_channels_t *channels) {
    header.checksum = 0;
    uint64_t hash =
        hash_bytes(UINT64_C(14695981039346656037), &header, sizeof(header));
    hash = hash_bytes(hash, payload->control, sizeof(payload->control));
    hash = hash_bytes(hash, payload->bodies, (size_t)header.body_words*4u);
    hash = hash_bytes(hash, payload->graph, sizeof(payload->graph));
    if(header.version>=7) hash=hash_bytes(hash,payload->states,
        (size_t)payload->body_count*sizeof(*payload->states));
    hash =
        hash_bytes(hash, pixels, (size_t)header.reserved[1] * sizeof(*pixels));
    hash=hash_bytes(hash,chunks,(size_t)header.reserved[0]*sizeof(*chunks));
    if(header.version>=6) {
        hash=hash_bytes(hash,body_colors,(size_t)header.reserved[1]*128);
        hash=hash_bytes(hash,colors,(size_t)header.reserved[0]*sizeof(*colors));
    }
    if(header.version>=8) hash=dc_checkpoint_channels_hash(hash,channels);
    return hash;
}

static dc_structural_store_payload_t *payload_create(uint32_t count) {
    if(!count||count>UINT32_MAX/sizeof(dc_gpu_body_record_t)) return NULL;
    dc_structural_store_payload_t *p=calloc(1,sizeof(*p));
    if(!p) return NULL;
    p->body_count=count;p->bodies=calloc(count,sizeof(*p->bodies));
    p->states=calloc(count,sizeof(*p->states));
    if(!p->bodies||!p->states) {free(p->bodies);free(p->states);free(p);return NULL;}
    return p;
}
static void payload_destroy(dc_structural_store_payload_t *p) {
    if(p) {free(p->bodies);free(p->states);free(p);}
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
    memset(graph + 36257u, 0, (38000u - 36257u) * sizeof(uint32_t));
    memset(graph + 39000u, 0, 1000u * sizeof(uint32_t));
    for (unsigned i = 0; i < 192; i++)
        memset(graph + 30000u + i * 20u + 15u, 0, 3 * sizeof(uint32_t));
}

bool fr_gpu_save_structure(dc_gpu_t *gpu, const char *path, char *err,
                           uint32_t cap) {
    if (!gpu || !path || !*path)
        return error(err, cap, "Invalid structure snapshot path");
    if(err&&cap) err[0]='\0';
    if (gpu->body_refresh_pending &&
        !dc_gpu_set_body_origin(gpu, gpu->body_origin, err, cap))
        return false;
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "Cannot reach snapshot boundary");
    char temporary[1024];
    int length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return error(err, cap, "Snapshot path too long");
    dc_structural_store_payload_t *payload = payload_create(dc_gpu_body_capacity(gpu));
    if (!payload)
        return error(err, cap, "Cannot allocate snapshot staging");
    uint32_t *data = dc_gpu_structural_data(gpu);
    payload->control[0] = data[0];
    payload->control[1] = data[3];
    payload->control[2] = data[6];
    payload->control[3] = data[14];
    for(uint32_t i=0;i<payload->body_count;i++) {
        payload->bodies[i]=*dc_gpu_body_record(gpu,i);
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(gpu,i);
        if(owner) payload->states[i].owner=*owner;
        if(gpu->runtime_body_consumers) memcpy(payload->states[i].persistent,
            dc_gpu_body_solver_state(gpu,i)+52u,sizeof(payload->states[i].persistent));
        else if(i<64u) {
            const uint32_t *g=data+dc_gpu_structural_graph_offset();
            memcpy(payload->states[i].persistent,g+38000u+i*4u,16);
            memcpy(payload->states[i].persistent+4,g+38500u+i*4u,16);
            memcpy(payload->states[i].persistent+8,g+36000u+i*4u,16);
        }
    }
    memcpy(payload->graph, data + dc_gpu_structural_graph_offset(),
           sizeof(payload->graph));
    clear_transients(payload->graph);
    dc_structural_store_header_t header = {
        .magic = {'D', 'C', 'S', '1'},
        .version = fr_gpu_revolute_span(gpu)?9u:8u,
        .body_words = payload->body_count * (sizeof(dc_gpu_body_record_t)/4u),
        .graph_words = DC_STRUCTURAL_RESERVE_WORDS,
        .flags = 1u | (gpu->impact_fracture_enabled ? 2u : 0u) |
                 (gpu->automatic_components_enabled ? 4u : 0u),
        .origin = gpu->body_origin};
    uint32_t capacity = dc_gpu_pixel_pool_capacity(gpu);
    uint32_t *pixels = calloc(capacity, sizeof(*pixels));
    float *body_colors=calloc((size_t)capacity*32,sizeof(float));
    bool okay = pixels != NULL && body_colors != NULL;
    for (unsigned i = 0; okay && i < payload->body_count; i++) {
        dc_gpu_body_record_t *b = payload->bodies + i;
        if ((!b->body.active && !b->reserved[0]) ||
            (!(b->piece_count & DC_GPU_PIXEL_BODY_FLAG)&&!(b->motion_flags&DC_BODY_MOTION_THERMAL_GRID)))
            continue;
        okay = b->compound_reserved >= dc_gpu_pixel_pool_word_offset(gpu) +
                                           DC_GPU_PIXEL_POOL_HEADER_WORDS &&
               (uint64_t)b->compound_reserved + 8u <=
                   dc_gpu_body_storage_bytes(gpu) / 4;
        if (!okay)
            break;
        uint32_t words = ((uint32_t *)gpu->body_mapped)[b->compound_reserved];
        okay = (uint64_t)b->compound_reserved + words <=
                   dc_gpu_body_storage_bytes(gpu) / 4 &&
               words <= capacity - header.reserved[1];
        if (!okay)
            break;
        memcpy(pixels + header.reserved[1],
               (uint32_t *)gpu->body_mapped + b->compound_reserved,
               words * sizeof(*pixels));
        memcpy(body_colors+(size_t)header.reserved[1]*32,
            dc_gpu_shape_colors(gpu,b->compound_reserved),(size_t)words*128);
        b->compound_reserved = header.reserved[1];
        header.reserved[1] += words;
    }
    okay = okay && dc_gpu_structure_payload_valid(gpu, payload, pixels,
                                                  header.reserved[1]);
    dc_chunk_t *chunks = calloc(
        DC_GPU_CHUNK_SLOTS + gpu->checkpoint_chunk_count, sizeof(*chunks));
    dc_chunk_colors_t *colors=calloc(DC_GPU_CHUNK_SLOTS+gpu->checkpoint_chunk_count,sizeof(*colors));
    okay = okay && chunks != NULL && colors != NULL;
    if (okay && gpu->checkpoint_chunk_count) {
        memcpy(chunks, gpu->checkpoint_chunks,
               (size_t)gpu->checkpoint_chunk_count * sizeof(*chunks));
        header.reserved[0] = gpu->checkpoint_chunk_count;
        if(gpu->checkpoint_colors) memcpy(colors,gpu->checkpoint_colors,
            (size_t)gpu->checkpoint_chunk_count*sizeof(*colors));
    }
    for (unsigned slot = 0; okay && slot < DC_GPU_CHUNK_SLOTS; slot++) {
        const uint32_t *record = data + DC_STRUCTURAL_HEADER_WORDS + slot * 8u;
        if (!record[5])
            continue;
        dc_chunk_t *chunk = chunks + header.reserved[0];
        memcpy(&chunk->coord, record, sizeof(chunk->coord));
        okay = dc_gpu_download_chunk(gpu, slot, chunk, err, cap) &&
               dc_gpu_checkpoint_chunk_valid(chunk);
        if (okay) {
            dc_gpu_read_chunk_colors(gpu,slot,colors+header.reserved[0]);
            header.reserved[0]++;
        }
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
    okay=okay && dc_color_records_valid(body_colors,(size_t)header.reserved[1]*8);
    for(unsigned c=0;okay&&c<header.reserved[0];c++)
        okay=dc_color_records_valid((const float*)(colors+c),DC_CHUNK_CELLS*2);
    dc_checkpoint_channels_t channels={0};
    if(okay) okay=dc_checkpoint_channels_collect(gpu,chunks,header.reserved[0],&channels,err,cap);
    if(okay) okay=fr_gpu_reactive_checkpoint_mass_valid(channels.reactive_length?&channels.reactive:NULL,
        payload,pixels,header.reserved[1],err,cap);
    if (okay)
        header.checksum = checksum(header, payload, pixels, chunks,body_colors,colors,&channels);
    FILE *file = okay ? fopen(temporary, "wb") : NULL;
    if (file) {
        okay = fwrite(&header, sizeof(header), 1, file) == 1 &&
               fwrite(payload->control,sizeof(payload->control),1,file)==1 &&
               fwrite(payload->bodies,4,header.body_words,file)==header.body_words &&
               fwrite(payload->graph,sizeof(payload->graph),1,file)==1 &&
               fwrite(payload->states,sizeof(*payload->states),payload->body_count,file)==payload->body_count &&
               fwrite(pixels, sizeof(*pixels), header.reserved[1], file) ==
                   header.reserved[1] &&
               dc_gpu_checkpoint_write_chunks(file, chunks, header.reserved[0]) &&
               fwrite(body_colors,128,header.reserved[1],file)==header.reserved[1] &&
               fwrite(colors,sizeof(*colors),header.reserved[0],file)==header.reserved[0] &&
               dc_checkpoint_channels_write(file,&channels);
        if (fclose(file) != 0)
            okay = false;
        if (okay)
            okay = rename(temporary, path) == 0;
        if (!okay)
            remove(temporary);
    } else
        okay = false;
    payload_destroy(payload);
    dc_checkpoint_channels_discard(gpu,&channels);
    free(pixels);
    free(chunks);
    free(body_colors);free(colors);
    if(!okay&&err&&cap&&err[0]) return false;
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
    for(uint32_t i=0;i<dc_gpu_body_capacity(gpu);i++) {
        dc_gpu_body_record_t *body=dc_gpu_body_record(gpu,i);
        memset(body,0,sizeof(*body));
        if(i<payload->body_count) *body=payload->bodies[i];
        if(gpu->runtime_body_consumers) {
            uint32_t *state=dc_gpu_body_solver_state(gpu,i);memset(state,0,256);
            dc_body_storage_owner_t *owner=dc_gpu_body_owner(gpu,i);memset(owner,0,sizeof(*owner));
            if(i<payload->body_count) {
                *owner=payload->states[i].owner;
                memcpy(state+52,payload->states[i].persistent,sizeof(payload->states[i].persistent));
            }
        }
    }
    uint32_t offset = dc_gpu_pixel_pool_word_offset(gpu);
    uint32_t *pool = (uint32_t *)gpu->body_mapped + offset;
    memset(pool, 0, DC_GPU_PIXEL_POOL_HEADER_WORDS * sizeof(*pool));
    pool[1] = header->reserved[1];
    pool[2] = dc_gpu_pixel_pool_capacity(gpu);
    memcpy(pool + DC_GPU_PIXEL_POOL_HEADER_WORDS, pixels,
           (size_t)header->reserved[1] * sizeof(*pixels));
    for (unsigned i = 0; i < payload->body_count; i++) {
        dc_gpu_body_record_t *b = dc_gpu_body_record(gpu,i);
        if (!b->body.active && !b->reserved[0])
            memset(b, 0, sizeof(*b));
        else if ((b->piece_count & DC_GPU_PIXEL_BODY_FLAG)||(b->motion_flags&DC_BODY_MOTION_THERMAL_GRID))
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
    gpu->structural_graph_possible = payload->graph[1] != 0 || data[9] != 0;
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
    dc_structural_store_payload_t *payload = NULL;
    dc_checkpoint_channels_t channels={0};
    bool okay =
        framed && fread(&header, sizeof(header), 1, file) == 1 &&
        memcmp(header.magic, "DCS1", 4) == 0 && header.version >= 1 &&
        header.version <= 9 && header.body_words &&
        header.body_words%(sizeof(dc_gpu_body_record_t)/4u)==0 &&
        (header.version>=7 || header.body_words==STORE_BODY_WORDS) &&
        header.graph_words == DC_STRUCTURAL_RESERVE_WORDS &&
        (header.flags & 1u) && !(header.flags & ~7u) &&
        header.reserved[0] <= DC_GPU_CHUNK_SLOTS * 2u &&
        (header.version >= 2 || !header.reserved[0]) &&
        (header.version >= 4 || !header.reserved[1]) &&
        header.reserved[1] <= UINT32_MAX/128u &&
        !header.reserved[2];
    if(okay) payload=payload_create(header.body_words/(sizeof(dc_gpu_body_record_t)/4u));
    okay=okay && payload && fread(payload->control,sizeof(payload->control),1,file)==1 &&
        fread(payload->bodies,4,header.body_words,file)==header.body_words &&
        fread(payload->graph,sizeof(payload->graph),1,file)==1;
    if(okay&&header.version>=7) okay=fread(payload->states,sizeof(*payload->states),
        payload->body_count,file)==payload->body_count;
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
                   header.reserved[0]);
    float *body_colors=okay?calloc((size_t)header.reserved[1]*32+1,sizeof(float)):NULL;
    dc_chunk_colors_t *colors=okay?calloc(header.reserved[0]+1,sizeof(*colors)):NULL;
    okay=okay && body_colors && colors;
    if(okay && header.version>=6)
        okay=fread(body_colors,128,header.reserved[1],file)==header.reserved[1] &&
            fread(colors,sizeof(*colors),header.reserved[0],file)==header.reserved[0];
    if(okay&&header.version>=8) okay=dc_checkpoint_channels_read(file,header.version,&channels);
    okay=okay && fgetc(file)==EOF && !ferror(file);
    okay=okay && dc_color_records_valid(body_colors,(size_t)header.reserved[1]*8);
    for(unsigned c=0;okay&&c<header.reserved[0];c++)
        okay=dc_color_records_valid((const float*)(colors+c),DC_CHUNK_CELLS*2);
    for (unsigned i = 0; okay && i < header.reserved[0]; i++) {
        okay = dc_gpu_checkpoint_chunk_valid(chunks + i);
        for (unsigned j = 0; okay && j < i; j++)
            okay = chunks[i].coord.x != chunks[j].coord.x ||
                   chunks[i].coord.y != chunks[j].coord.y;
    }
    if (fclose(file) != 0)
        okay = false;
    okay = okay && header.checksum == checksum(header, payload, pixels, chunks,body_colors,colors,&channels);
    if (okay && header.version == 4)
        okay = dc_gpu_upgrade_dense_pixels(gpu, payload->bodies, &pixels,
                                           &header.reserved[1]);
    if(okay&&header.version<7) for(uint32_t i=0;i<payload->body_count;i++) {
        memcpy(payload->states[i].persistent,payload->graph+38000u+i*4u,16);
        memcpy(payload->states[i].persistent+4,payload->graph+38500u+i*4u,16);
        memcpy(payload->states[i].persistent+8,payload->graph+36000u+i*4u,16);
    }
    okay = okay && dc_gpu_structure_payload_valid(gpu, payload, pixels,
                                                  header.reserved[1]);
    if (!okay) {
        dc_checkpoint_channels_discard(gpu,&channels);
        payload_destroy(payload);
        free(pixels);
        free(chunks);
        free(body_colors);free(colors);
        return error(err, cap, "Invalid or truncated structure snapshot");
    }
    bool graph_solver=payload->graph[0]!=0u||payload->graph[1]!=0u;
    for(unsigned bond=0;bond<192u;bond++) graph_solver|=payload->graph[30000u+bond*20u]!=0u;
    if(graph_solver&&!dc_gpu_solver_graph_prepare(gpu,err,cap)) {
        dc_checkpoint_channels_discard(gpu,&channels);
        payload_destroy(payload);free(pixels);free(chunks);free(body_colors);free(colors);return false;
    }
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS) {
        dc_checkpoint_channels_discard(gpu,&channels);
        payload_destroy(payload);
        free(pixels);
        free(chunks);
        free(body_colors);free(colors);
        return error(err, cap, "Cannot reach restore boundary");
    }
    if(!dc_gpu_runtime_bodies_reserve(gpu,payload->body_count,err,cap)||
       !dc_gpu_runtime_geometry_reserve(gpu,header.reserved[1],err,cap)) {
        dc_checkpoint_channels_discard(gpu,&channels);
        payload_destroy(payload);free(pixels);free(chunks);free(body_colors);free(colors);return false;
    }
    if(header.version<7) for(uint32_t i=0;i<payload->body_count;i++) {
        memcpy(payload->states[i].persistent,payload->graph+38000u+i*4u,16);
        memcpy(payload->states[i].persistent+4,payload->graph+38500u+i*4u,16);
        memcpy(payload->states[i].persistent+8,payload->graph+36000u+i*4u,16);
    }
    if(!dc_checkpoint_channels_prepare(gpu,&channels,chunks,header.reserved[0],payload,pixels,header.reserved[1],err,cap)) {
        dc_checkpoint_channels_discard(gpu,&channels);payload_destroy(payload);free(pixels);free(chunks);free(body_colors);free(colors);return false;
    }
    dc_chunk_coord_t origin = gpu->body_origin;
    gpu->reactive_checkpoint_installing=true;
    install(gpu, &header, payload, pixels);
    memset(gpu->shape_color_mapped,0,(size_t)(dc_gpu_color_terrain_bytes()+2*dc_gpu_color_bank_bytes(gpu)));
    if(header.version>=6) memcpy((uint8_t*)gpu->shape_color_mapped+dc_gpu_color_terrain_bytes(),
        body_colors,(size_t)header.reserved[1]*128);
    free(body_colors);
    payload_destroy(payload);
    free(pixels);
    dc_checkpoint_channels_publish(gpu,&channels,chunks,colors,header.reserved[0]);
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
            okay = dc_gpu_upload_chunk_checkpoint(gpu, slot, chunk, false,NULL,err,cap);
            break;
        }
    }
    okay = okay && dc_gpu_set_rigid_solver(gpu, true) &&
           dc_gpu_set_body_origin(gpu, origin, err, cap) &&
           refresh_topology(gpu);
    gpu->reactive_checkpoint_installing=false;
    dc_checkpoint_channels_discard(gpu,&channels);
    return okay || error(err, cap, "Cannot restore structural GPU state");
}
