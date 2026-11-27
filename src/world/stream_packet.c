#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../fire/gas_checkpoint_internal.h"
#include "dungeoncraft/generate.h"
#include "ferrum/fire/thermal_checkpoint.h"
#include "stream_packet.h"

#define DC_LEGACY_GAS_CHECKPOINT_BYTES offsetof(fr_gas_checkpoint_chunk_t,carrier_tail)

typedef struct {
    char magic[4];
    uint32_t version;
    int64_t x, y;
    uint64_t seed;
} chunk_header_t;

/* The timed envelope leaves all existing channel payload layouts intact. */
enum { CHUNK_TIMED_VERSION = 12, CHUNK_TAKEOVER_VERSION = 13 };
typedef struct {
    uint32_t payload_version, reserved;
    double completed_seconds;
} chunk_time_header_t;

_Static_assert(sizeof(chunk_header_t) == 32, "Chunk file header layout changed");
_Static_assert(sizeof(chunk_time_header_t) == 16, "Chunk time header layout changed");
_Static_assert(sizeof(dc_chunk_owner_t) == 16, "Chunk owner record layout changed");

static bool chunk_path(const char *directory, dc_chunk_coord_t coord,
                       char *path, size_t cap, bool temporary) {
    int count = snprintf(path, cap, "%s/chunk_%016" PRIx64 "_%016" PRIx64 ".bin%s",
        directory, (uint64_t)coord.x, (uint64_t)coord.y,
        temporary ? ".tmp" : "");
    return count > 0 && (size_t)count < cap;
}

static bool prefix_hash(FILE *file,long end,uint64_t *out) {
    if(end<0||fseek(file,0,SEEK_SET)!=0) return false;
    uint64_t value=UINT64_C(14695981039346656037);uint8_t bytes[4096];
    while(end>0) {
        size_t count=(unsigned long)end<sizeof(bytes)?(size_t)end:sizeof(bytes);
        if(fread(bytes,1,count,file)!=count) return false;
        value=dc_color_hash(value,bytes,count);end-=(long)count;
    }
    *out=value;return true;
}

static bool verify_chunk_hash(FILE *file) {
    long end = ftell(file);
    uint64_t stored, actual;
    return fread(&stored, sizeof(stored), 1, file) == 1 &&
           fgetc(file) == EOF && !ferror(file) &&
           prefix_hash(file, end, &actual) && actual == stored;
}

bool dc_stream_write_packet(const char *root, uint64_t seed, const stream_job_t *job) {
    const dc_chunk_t *chunk = job->chunk;
    const dc_chunk_colors_t *colors = job->colors;
    const fr_reactive_chunk_t *reactive = job->reactive;
    const uint8_t *thermal = job->thermal, *bulk_packet = job->bulk_packet;
    const size_t thermal_bytes = job->thermal_bytes, bulk_bytes = job->bulk_bytes;
    const fr_gas_checkpoint_chunk_t *gas = job->gas;
    const dc_stream_chunk_time_t time = job->time;
    const dc_chunk_owner_t replaces_owner = job->replaces_owner;
    const char *staging = job->staging;
    dc_chunk_colors_t ambient_colors={0};
    if(reactive&&!colors) colors=&ambient_colors;
    char temporary[1024], final[1024];
    const char *directory = staging ? staging : root;
    const bool takeover = dc_chunk_owner_valid(replaces_owner);
    if (!chunk_path(directory, chunk->coord, temporary, sizeof(temporary), true) ||
        !chunk_path(directory, chunk->coord, final, sizeof(final), false)) return false;
    FILE *file = fopen(temporary, "w+b");
    if (!file) return false;
    chunk_header_t header = { .magic = {'D', 'C', 'C', '1'}, .version = 5,
        .x = chunk->coord.x, .y = chunk->coord.y, .seed = seed };
    if(colors) header.version=6;
    if(reactive) header.version=7;
    if(thermal) header.version=8;
    if(gas) header.version=11;
    if(bulk_packet) header.version=10;
    chunk_time_header_t time_header = {.payload_version = header.version,
                                       .completed_seconds = time.known ? time.seconds : 0.0};
    if (takeover) header.version = CHUNK_TAKEOVER_VERSION;
    else if (time.known) header.version = CHUNK_TIMED_VERSION;
    bool okay = fwrite(&header, sizeof(header), 1, file) == 1 &&
        (!time.known || fwrite(&time_header, sizeof(time_header), 1, file) == 1) &&
        (!takeover || fwrite(&replaces_owner, sizeof(replaces_owner), 1, file) == 1) &&
        fwrite(chunk->cells, sizeof(chunk->cells), 1, file) == 1 &&
        chunk->marker_count <= DC_MARKERS_PER_CHUNK &&
        chunk->particle_count <= DC_MPM_PARTICLES_PER_CHUNK &&
        fwrite(&chunk->marker_count, sizeof(chunk->marker_count), 1, file) == 1 &&
        fwrite(chunk->markers, sizeof(dc_marker_t), chunk->marker_count, file) ==
            chunk->marker_count &&
        fwrite(chunk->face_velocity, sizeof(chunk->face_velocity), 1, file) == 1 &&
        fwrite(&chunk->particle_count, sizeof(chunk->particle_count), 1, file) == 1;
    uint32_t written = 0;
    for (uint32_t i = 0; okay && i < DC_MPM_PARTICLES_PER_CHUNK; ++i) {
        if (!chunk->particles[i].mass_fp) continue;
        okay = fwrite(&i, sizeof(i), 1, file) == 1 &&
            fwrite(&chunk->particles[i], sizeof(dc_mpm_particle_t), 1, file) == 1;
        ++written;
    }
    okay = okay && written == chunk->particle_count;
    if(okay && colors) {
        uint64_t hash=dc_color_hash(14695981039346656037ull,chunk->cells,sizeof(chunk->cells));
        hash=dc_color_hash(hash,colors,sizeof(*colors));
        okay=dc_color_records_valid((const float*)colors,DC_CHUNK_CELLS*2) &&
            fwrite(colors,sizeof(*colors),1,file)==1 && fwrite(&hash,sizeof(hash),1,file)==1;
    }
    if(okay&&reactive) {
        size_t size=0,written=0;uint8_t *bytes=NULL;
        okay=fr_reactive_chunk_size(reactive->count,&size)&&(bytes=malloc(size))!=NULL&&
            fr_reactive_chunk_encode(reactive,chunk,bytes,size,&written,NULL,0);
        uint64_t length=size;
        if(okay) okay=fwrite(&length,sizeof(length),1,file)==1&&fwrite(bytes,size,1,file)==1;
        free(bytes);
        if(okay&&(thermal||gas||bulk_packet)) {
            uint64_t liquid_length=thermal_bytes;
            okay=fwrite(&liquid_length,sizeof(liquid_length),1,file)==1&&
                (!thermal_bytes||fwrite(thermal,thermal_bytes,1,file)==1);
        }
        if(okay&&bulk_packet) {
            uint64_t gas_length=gas?sizeof(*gas):0u,liquid_length=bulk_bytes;
            okay=fwrite(&gas_length,sizeof(gas_length),1,file)==1&&
                (!gas||fwrite(gas,sizeof(*gas),1,file)==1)&&
                fwrite(&liquid_length,sizeof(liquid_length),1,file)==1&&
                fwrite(bulk_packet,bulk_bytes,1,file)==1;
        } else if(okay&&gas) okay=fwrite(gas,sizeof(*gas),1,file)==1;
    }
    if (okay && (reactive || time.known)) {
        long end = ftell(file);
        uint64_t hash;
        okay = prefix_hash(file, end, &hash) && fseek(file, end, SEEK_SET) == 0 &&
               fwrite(&hash, sizeof(hash), 1, file) == 1;
    }
    if (okay && takeover && !staging) okay = fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file) != 0) okay = false;
    if (okay) okay = rename(temporary, final) == 0;
    if (okay && takeover && !staging) {
        int directory_fd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        okay = directory_fd >= 0 && fsync(directory_fd) == 0;
        if (directory_fd >= 0) close(directory_fd);
    }
    if (!okay) remove(temporary);
    return okay;
}

static bool load_gas_checkpoint(FILE *file,size_t bytes,fr_gas_checkpoint_chunk_t **gas) {
    *gas=calloc(1,sizeof(**gas));
    return *gas&&fread(*gas,bytes,1,file)==1&&gas_checkpoint_valid(*gas);
}

static dc_chunk_t *load_chunk(const char *root, uint64_t seed, const dc_ground_profile_t *ground,
    const char *staging, dc_chunk_coord_t coord,
    dc_chunk_colors_t *colors,fr_reactive_chunk_t **reactive,float **water_excess,
    fr_gas_checkpoint_chunk_t **gas,fr_bulk_phase_chunk_t **bulk,dc_stream_chunk_time_t *completed,
    dc_chunk_owner_t *replaces_owner, bool *generated) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return NULL;
    chunk->coord=coord;
    char path[1024];
    if (!chunk_path(staging?staging:root, coord, path, sizeof(path), false)) { free(chunk); return NULL; }
    FILE *file = fopen(path, "rb");
    if (!file && errno == ENOENT && staging) {
        if (!chunk_path(root, coord, path, sizeof(path), false)) { free(chunk); return NULL; }
        file = fopen(path, "rb");
    }
    if (!file && errno == ENOENT) {
        bool okay = true;
        if (ground) {
            okay = dc_generate_chunk_ground(coord, ground, chunk);
        } else {
#ifdef DC_NATIVE_VIEW
            okay = dc_generate_chunk_scaled(seed, coord, 4u, chunk);
#else
            dc_generate_chunk(seed, coord, chunk);
#endif
        }
        if (!okay) {
            free(chunk);
            return NULL;
        }
        *generated = true;
        return chunk;
    }
    if (!file) { free(chunk); return NULL; }
    chunk_header_t header = {0};
    bool okay = fread(&header, sizeof(header), 1, file) == 1 &&
        memcmp(header.magic, "DCC1", 4) == 0 &&
        (header.version >= 1 && header.version <= CHUNK_TAKEOVER_VERSION) &&
        header.x == coord.x && header.y == coord.y && header.seed == seed;
    uint32_t version = header.version;
    dc_stream_chunk_time_t time = {0};
    dc_chunk_owner_t owner = {0};
    if (okay && version >= CHUNK_TIMED_VERSION) {
        chunk_time_header_t time_header;
        okay = fread(&time_header, sizeof(time_header), 1, file) == 1 &&
               time_header.payload_version >= 5 && time_header.payload_version <= 11 &&
               !time_header.reserved && dc_stream_valid_completed_time(time_header.completed_seconds);
        if (okay && version == CHUNK_TAKEOVER_VERSION) {
            okay = fread(&owner, sizeof(owner), 1, file) == 1 && dc_chunk_owner_valid(owner);
        }
        if (okay) {
            version = time_header.payload_version;
            time = (dc_stream_chunk_time_t){.seconds = time_header.completed_seconds, .known = true};
        }
    }
    if (okay) okay = fread(chunk->cells, sizeof(chunk->cells), 1, file) == 1;
    if (okay && version >= 2)
        okay = fread(&chunk->marker_count, sizeof(chunk->marker_count), 1, file) == 1 &&
            chunk->marker_count <= DC_MARKERS_PER_CHUNK &&
            fread(chunk->markers, sizeof(dc_marker_t), chunk->marker_count, file) ==
                chunk->marker_count;
    if (okay && version >= 3)
        okay = fread(chunk->face_velocity, sizeof(chunk->face_velocity), 1, file) == 1;
    if (okay && version >= 4) {
        okay = fread(&chunk->particle_count, sizeof(chunk->particle_count), 1, file) == 1 &&
            chunk->particle_count <= DC_MPM_PARTICLES_PER_CHUNK;
        for (uint32_t n = 0; okay && n < chunk->particle_count; ++n) {
            uint32_t index;
            dc_mpm_particle_t particle;
            okay = fread(&index, sizeof(index), 1, file) == 1 &&
                fread(&particle, sizeof(particle), 1, file) == 1 &&
                index < DC_MPM_PARTICLES_PER_CHUNK && particle.mass_fp != 0;
            if (okay && chunk->particles[index].mass_fp) okay = false;
            if (okay) {
                if (version == 4)
                    particle.grain_fp = dc_chunk_grain_radius_fp(particle.material);
                chunk->particles[index] = particle;
            }
        }
    }
    if(okay && version>=6) {
        uint64_t stored=0,hash=dc_color_hash(14695981039346656037ull,chunk->cells,sizeof(chunk->cells));
        okay=fread(colors,sizeof(*colors),1,file)==1 && fread(&stored,sizeof(stored),1,file)==1 &&
            dc_color_records_valid((const float*)colors,DC_CHUNK_CELLS*2);
        hash=dc_color_hash(hash,colors,sizeof(*colors));
        okay=okay&&hash==stored;
        if(okay&&version==6&&!time.known) okay=fgetc(file)==EOF&&!ferror(file);
    }
    if (okay && version < 7 && time.known) okay = verify_chunk_hash(file);
    if(okay&&version>=7) {
        uint64_t length=0;size_t maximum=0;uint8_t *bytes=NULL;
        okay=fr_reactive_chunk_size(DC_CHUNK_CELLS+DC_MPM_PARTICLES_PER_CHUNK,&maximum)&&
            fread(&length,sizeof(length),1,file)==1&&length>=72u&&length<=maximum;
        if(okay) okay=(bytes=malloc((size_t)length))!=NULL&&fread(bytes,(size_t)length,1,file)==1;
        uint64_t liquid_length=0;uint8_t *liquid=NULL;
        if(okay&&version>=8) {
            enum { LIQUID_CHUNK_BYTES=48+16+DC_CHUNK_CELLS*4 };
            okay=fread(&liquid_length,sizeof(liquid_length),1,file)==1&&
                (liquid_length==LIQUID_CHUNK_BYTES||(version>=9&&liquid_length==0u));
            if(okay&&liquid_length) okay=(liquid=malloc((size_t)liquid_length))!=NULL&&fread(liquid,(size_t)liquid_length,1,file)==1;
        }
        if(okay&&version==9) okay=load_gas_checkpoint(file,DC_LEGACY_GAS_CHECKPOINT_BYTES,gas);
        if(okay&&version==11) okay=load_gas_checkpoint(file,sizeof(**gas),gas);
        if(okay&&version==10) {
            uint64_t gas_length=0,bulk_length=0;uint8_t *packet=NULL;
            okay=fread(&gas_length,sizeof(gas_length),1,file)==1&&
                (gas_length==0u||gas_length==DC_LEGACY_GAS_CHECKPOINT_BYTES||gas_length==sizeof(**gas));
            if(okay&&gas_length) okay=load_gas_checkpoint(file,(size_t)gas_length,gas);
            if(okay) okay=fread(&bulk_length,sizeof(bulk_length),1,file)==1&&
                bulk_length>0u&&bulk_length<=fr_bulk_phase_chunk_bytes();
            if(okay) okay=(packet=malloc((size_t)bulk_length))!=NULL&&
                fread(packet,(size_t)bulk_length,1,file)==1&&
                (*bulk=malloc(sizeof(**bulk)))!=NULL&&
                fr_bulk_phase_chunk_decode(chunk,packet,(size_t)bulk_length,*bulk);
            free(packet);
        }
        if(okay) okay=verify_chunk_hash(file);
        if(okay) okay=(*reactive=calloc(1,sizeof(**reactive)))!=NULL&&
            fr_reactive_chunk_decode(bytes,(size_t)length,chunk,*reactive,NULL,0);
        if(okay&&liquid) {
            fr_thermal_checkpoint_t state={0};
            okay=fr_thermal_checkpoint_decode(liquid,(size_t)liquid_length,chunk,1,&state,NULL,0)&&
                state.purpose==FR_THERMAL_WORLD_GENERIC&&state.source.excess_energy==0.f;
            if(okay) {*water_excess=state.water_excess;state.water_excess=NULL;}
            fr_thermal_checkpoint_release(&state);
        }
        free(bytes);free(liquid);
    }
    if (fclose(file) != 0) okay = false;
    if (!okay) {dc_stream_release_channel(*reactive);*reactive=NULL;free(*water_excess);*water_excess=NULL;
        free(*gas);*gas=NULL;free(*bulk);*bulk=NULL;free(chunk);return NULL;}
    chunk->coord = coord;
    if (version < 4) dc_chunk_seed_particles(chunk);
    *completed = time;
    *replaces_owner = owner;
    return chunk;
}

bool dc_stream_read_packet(const char *root, uint64_t seed, const dc_ground_profile_t *ground,
    const char *staging, dc_stream_result_t *result) {
    result->colors = calloc(1, sizeof(*result->colors));
    if (!result->colors) return false;
    result->chunk = load_chunk(root, seed, ground, staging, result->coord,
        result->colors, &result->reactive, &result->water_excess, &result->gas,
        &result->bulk, &result->time, &result->replaces_owner, &result->generated);
    return result->chunk != NULL;
}
