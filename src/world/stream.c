#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>

#include "dungeoncraft/generate.h"
#include "dungeoncraft/stream.h"
#include "ferrum/fire/thermal_checkpoint.h"
#include "../fire/gas_checkpoint_internal.h"

#define DC_LEGACY_GAS_CHECKPOINT_BYTES offsetof(fr_gas_checkpoint_chunk_t,carrier_tail)

typedef enum { JOB_LOAD, JOB_SAVE } job_kind_t;
typedef struct {
    job_kind_t kind;
    dc_chunk_coord_t coord;
    uint64_t generation;
    const char *staging;
    dc_chunk_t *chunk;
    dc_chunk_colors_t *colors;
    fr_reactive_chunk_t *reactive;
    uint8_t *thermal;
    size_t thermal_bytes;
    fr_gas_checkpoint_chunk_t *gas;
    uint8_t *bulk_packet;
    size_t bulk_bytes;
    dc_stream_chunk_time_t time;
} stream_job_t;

typedef struct {
    char magic[4];
    uint32_t version;
    int64_t x, y;
    uint64_t seed;
} chunk_header_t;

/* The timed envelope leaves all existing channel payload layouts intact. */
enum { CHUNK_TIMED_VERSION = 12 };
typedef struct {
    uint32_t payload_version, reserved;
    double completed_seconds;
} chunk_time_header_t;

_Static_assert(sizeof(chunk_header_t) == 32, "Chunk file header layout changed");
_Static_assert(sizeof(chunk_time_header_t) == 16, "Chunk time header layout changed");

struct dc_streamer {
    char *directory;
    char *staging;
    uint64_t seed;
    uint32_t capacity;
    dc_ground_profile_t ground;
    uint32_t *ground_surface;
    stream_job_t *jobs;
    dc_stream_result_t *results;
    uint32_t job_head, job_tail, job_count;
    uint32_t result_head, result_tail, result_count;
    mtx_t mutex;
    cnd_t jobs_ready;
    cnd_t results_space;
    thrd_t thread;
    bool stop;
};

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

static bool valid_completed_time(double seconds) {
    return isfinite(seconds) && seconds >= 0.0;
}
static void release_channel(fr_reactive_chunk_t *channel) {
    if(channel) {fr_reactive_chunk_release(channel);free(channel);}
}
static bool save_chunk(const dc_streamer_t *stream,const char *staging,const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors,const fr_reactive_chunk_t *reactive,
    const uint8_t *thermal,size_t thermal_bytes,const fr_gas_checkpoint_chunk_t *gas,
    const uint8_t *bulk_packet,size_t bulk_bytes,dc_stream_chunk_time_t time) {
    dc_chunk_colors_t ambient_colors={0};
    if(reactive&&!colors) colors=&ambient_colors;
    char temporary[1024], final[1024];
    const char *directory=staging?staging:stream->directory;
    if (!chunk_path(directory, chunk->coord, temporary, sizeof(temporary), true) ||
        !chunk_path(directory, chunk->coord, final, sizeof(final), false)) return false;
    FILE *file = fopen(temporary, "w+b");
    if (!file) return false;
    chunk_header_t header = { .magic = {'D', 'C', 'C', '1'}, .version = 5,
        .x = chunk->coord.x, .y = chunk->coord.y, .seed = stream->seed };
    if(colors) header.version=6;
    if(reactive) header.version=7;
    if(thermal) header.version=8;
    if(gas) header.version=11;
    if(bulk_packet) header.version=10;
    chunk_time_header_t time_header = {.payload_version = header.version,
                                       .completed_seconds = time.known ? time.seconds : 0.0};
    if (time.known) header.version = CHUNK_TIMED_VERSION;
    bool okay = fwrite(&header, sizeof(header), 1, file) == 1 &&
        (!time.known || fwrite(&time_header, sizeof(time_header), 1, file) == 1) &&
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
    if (fclose(file) != 0) okay = false;
    if (okay) okay = rename(temporary, final) == 0;
    if (!okay) remove(temporary);
    return okay;
}

static bool load_gas_checkpoint(FILE *file,size_t bytes,fr_gas_checkpoint_chunk_t **gas) {
    *gas=calloc(1,sizeof(**gas));
    return *gas&&fread(*gas,bytes,1,file)==1&&gas_checkpoint_valid(*gas);
}

static dc_chunk_t *load_chunk(const dc_streamer_t *stream,const char *staging,dc_chunk_coord_t coord,
    dc_chunk_colors_t *colors,fr_reactive_chunk_t **reactive,float **water_excess,
    fr_gas_checkpoint_chunk_t **gas,fr_bulk_phase_chunk_t **bulk,dc_stream_chunk_time_t *completed,
    bool *generated) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return NULL;
    chunk->coord=coord;
    char path[1024];
    if (!chunk_path(staging?staging:stream->directory, coord, path, sizeof(path), false)) { free(chunk); return NULL; }
    FILE *file = fopen(path, "rb");
    if (!file && errno == ENOENT && staging) {
        if (!chunk_path(stream->directory, coord, path, sizeof(path), false)) { free(chunk); return NULL; }
        file = fopen(path, "rb");
    }
    if (!file && errno == ENOENT) {
        bool okay = true;
        if (stream->ground_surface) {
            okay = dc_generate_chunk_ground(coord, &stream->ground, chunk);
        } else {
#ifdef DC_NATIVE_VIEW
            okay = dc_generate_chunk_scaled(stream->seed, coord, 4u, chunk);
#else
            dc_generate_chunk(stream->seed, coord, chunk);
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
        (header.version >= 1 && header.version <= CHUNK_TIMED_VERSION) &&
        header.x == coord.x && header.y == coord.y && header.seed == stream->seed;
    uint32_t version = header.version;
    dc_stream_chunk_time_t time = {0};
    if (okay && version == CHUNK_TIMED_VERSION) {
        chunk_time_header_t time_header;
        okay = fread(&time_header, sizeof(time_header), 1, file) == 1 &&
               time_header.payload_version >= 5 && time_header.payload_version <= 11 &&
               !time_header.reserved && valid_completed_time(time_header.completed_seconds);
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
    if (!okay) {release_channel(*reactive);*reactive=NULL;free(*water_excess);*water_excess=NULL;
        free(*gas);*gas=NULL;free(*bulk);*bulk=NULL;free(chunk);return NULL;}
    chunk->coord = coord;
    if (version < 4) dc_chunk_seed_particles(chunk);
    *completed = time;
    return chunk;
}

static int worker_main(void *arg) {
    dc_streamer_t *stream = arg;
    for (;;) {
        mtx_lock(&stream->mutex);
        while (!stream->stop && stream->job_count == 0)
            cnd_wait(&stream->jobs_ready, &stream->mutex);
        if (stream->stop && stream->job_count == 0) { mtx_unlock(&stream->mutex); break; }
        stream_job_t job = stream->jobs[stream->job_head];
        memset(&stream->jobs[stream->job_head], 0, sizeof(job));
        stream->job_head = (stream->job_head + 1u) % stream->capacity;
        --stream->job_count;
        mtx_unlock(&stream->mutex);

        dc_stream_result_t result = { .coord = job.coord, .generation = job.generation };
        if (job.kind == JOB_LOAD) {
            result.colors=calloc(1,sizeof(*result.colors));
            if (result.colors) {
                result.chunk = load_chunk(stream, job.staging, job.coord, result.colors, &result.reactive,
                    &result.water_excess, &result.gas, &result.bulk, &result.time, &result.generated);
            }
            result.kind = result.chunk ? DC_STREAM_LOADED : DC_STREAM_FAILED;
        } else {
            result.kind = save_chunk(stream,job.staging,job.chunk,job.colors,job.reactive,job.thermal,job.thermal_bytes,
                job.gas,job.bulk_packet,job.bulk_bytes,job.time) ? DC_STREAM_SAVED : DC_STREAM_FAILED;
            free(job.chunk);
            free(job.colors);
            release_channel(job.reactive);
            free(job.thermal);
            free(job.gas);
            free(job.bulk_packet);
        }

        mtx_lock(&stream->mutex);
        while (!stream->stop && stream->result_count == stream->capacity)
            cnd_wait(&stream->results_space, &stream->mutex);
        if (stream->stop) {
            mtx_unlock(&stream->mutex);
            dc_stream_result_release(&result);
            continue;
        }
        stream->results[stream->result_tail] = result;
        stream->result_tail = (stream->result_tail + 1u) % stream->capacity;
        ++stream->result_count;
        mtx_unlock(&stream->mutex);
    }
    return 0;
}

dc_streamer_t *dc_stream_create(const char *directory, uint64_t seed,
                                uint32_t queue_capacity) {
    return dc_stream_create_ground(directory,seed,queue_capacity,NULL);
}

dc_streamer_t *dc_stream_create_ground(const char *directory, uint64_t seed,
                                       uint32_t queue_capacity, const dc_ground_profile_t *ground) {
    if (!directory || !directory[0] || strlen(directory) > 900 || !queue_capacity) return NULL;
    if (ground && (!ground->surface || !ground->width || ground->width>4194304u || !ground->soil_depth)) return NULL;
    if (mkdir(directory, 0755) != 0 && errno != EEXIST) return NULL;
    dc_streamer_t *stream = calloc(1, sizeof(*stream));
    if (!stream) return NULL;
    size_t length = strlen(directory) + 1;
    stream->directory = malloc(length);
    stream->jobs = calloc(queue_capacity, sizeof(*stream->jobs));
    stream->results = calloc(queue_capacity, sizeof(*stream->results));
    if (!stream->directory || !stream->jobs || !stream->results) goto fail_alloc;
    if (ground) {
        stream->ground_surface=malloc((size_t)ground->width*sizeof(uint32_t));
        if (!stream->ground_surface) goto fail_alloc;
        memcpy(stream->ground_surface,ground->surface,(size_t)ground->width*sizeof(uint32_t));
        stream->ground=*ground;stream->ground.surface=stream->ground_surface;
    }
    memcpy(stream->directory, directory, length);
    stream->seed = seed;
    stream->capacity = queue_capacity;
    if (mtx_init(&stream->mutex, mtx_plain) != thrd_success) goto fail_alloc;
    if (cnd_init(&stream->jobs_ready) != thrd_success) goto fail_mutex;
    if (cnd_init(&stream->results_space) != thrd_success) goto fail_jobs;
    if (thrd_create(&stream->thread, worker_main, stream) != thrd_success) goto fail_space;
    return stream;
fail_space:
    cnd_destroy(&stream->results_space);
fail_jobs:
    cnd_destroy(&stream->jobs_ready);
fail_mutex:
    mtx_destroy(&stream->mutex);
fail_alloc:
    free(stream->ground_surface);
    free(stream->directory); free(stream->jobs); free(stream->results); free(stream);
    return NULL;
}

bool dc_stream_stage(dc_streamer_t *stream, const char *directory) {
    if (!stream || !directory || !directory[0] || strlen(directory) > 950) return false;
    size_t length = strlen(directory) + 1;
    char *staging = malloc(length);
    if (!staging) return false;
    memcpy(staging, directory, length);
    mtx_lock(&stream->mutex);
    bool ready = !stream->stop && !stream->staging;
    if (ready) stream->staging = staging;
    mtx_unlock(&stream->mutex);
    if (!ready) free(staging);
    return ready;
}

static bool queue_job(dc_streamer_t *stream, stream_job_t job) {
    mtx_lock(&stream->mutex);
    if (stream->stop || stream->job_count == stream->capacity) {
        mtx_unlock(&stream->mutex);
        return false;
    }
    job.staging = stream->staging;
    stream->jobs[stream->job_tail] = job;
    stream->job_tail = (stream->job_tail + 1u) % stream->capacity;
    ++stream->job_count;
    cnd_signal(&stream->jobs_ready);
    mtx_unlock(&stream->mutex);
    return true;
}

bool dc_stream_request_load(dc_streamer_t *stream, dc_chunk_coord_t coord,
                            uint64_t generation) {
    if (!stream || !generation) return false;
    stream_job_t job = { .kind = JOB_LOAD, .coord = coord, .generation = generation };
    return queue_job(stream, job);
}

bool dc_stream_request_save(dc_streamer_t *stream, const dc_chunk_t *chunk,
                            uint64_t generation) {
    return dc_stream_request_save_colors(stream,chunk,NULL,generation);
}
bool dc_stream_request_save_colors(dc_streamer_t *stream,const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors,uint64_t generation) {
    return dc_stream_request_save_reactive(stream,chunk,colors,NULL,generation);
}
bool dc_stream_request_save_reactive(dc_streamer_t *stream,const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors,const fr_reactive_chunk_t *reactive,uint64_t generation) {
    return dc_stream_request_save_thermal(stream,chunk,colors,reactive,NULL,generation);
}
bool dc_stream_request_save_thermal(dc_streamer_t *stream,const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors,const fr_reactive_chunk_t *reactive,
    const float *water_excess,uint64_t generation) {
    return dc_stream_request_save_gas(stream,chunk,colors,reactive,water_excess,NULL,generation);
}
bool dc_stream_request_save_gas(dc_streamer_t *stream,const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors,const fr_reactive_chunk_t *reactive,
    const float *water_excess,const fr_gas_checkpoint_chunk_t *gas,uint64_t generation) {
    return dc_stream_request_save_bulk(stream,chunk,colors,reactive,water_excess,gas,NULL,generation);
}
bool dc_stream_request_save_bulk(dc_streamer_t *stream,const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors,const fr_reactive_chunk_t *reactive,
    const float *water_excess,const fr_gas_checkpoint_chunk_t *gas,
    const fr_bulk_phase_chunk_t *bulk,uint64_t generation) {
    return dc_stream_request_save_snapshot(stream,chunk,colors,reactive,water_excess,gas,bulk,
                                           (dc_stream_chunk_time_t){0},generation);
}
bool dc_stream_request_save_snapshot(dc_streamer_t *stream,const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors,const fr_reactive_chunk_t *reactive,
    const float *water_excess,const fr_gas_checkpoint_chunk_t *gas,
    const fr_bulk_phase_chunk_t *bulk,dc_stream_chunk_time_t time,uint64_t generation) {
    fr_reactive_chunk_t empty={.next_id=1};
    if((water_excess||gas||bulk)&&!reactive) reactive=&empty;
    if (!stream || !chunk || !generation) return false;
    if (time.known && !valid_completed_time(time.seconds)) return false;
    if(!gas_checkpoint_valid(gas)) return false;
    if(colors && !dc_color_records_valid((const float*)colors,DC_CHUNK_CELLS*2)) return false;
    if(reactive&&!fr_reactive_chunk_valid(reactive,chunk,NULL,0)) return false;
    dc_chunk_t *copy = malloc(sizeof(*copy));
    if (!copy) return false;
    memcpy(copy, chunk, sizeof(*copy));
    dc_chunk_colors_t *color_copy=colors?malloc(sizeof(*color_copy)):NULL;
    if(colors && !color_copy) {free(copy);return false;}
    if(colors) memcpy(color_copy,colors,sizeof(*colors));
    fr_reactive_chunk_t *reaction_copy=reactive?calloc(1,sizeof(*reaction_copy)):NULL;
    if(reactive) {
        if(!reaction_copy) {free(copy);free(color_copy);return false;}
        *reaction_copy=*reactive;reaction_copy->attachments=NULL;
        if(reactive->count) {
            size_t bytes=(size_t)reactive->count*sizeof(*reactive->attachments);
            reaction_copy->attachments=malloc(bytes);
            if(!reaction_copy->attachments) {free(copy);free(color_copy);free(reaction_copy);return false;}
            memcpy(reaction_copy->attachments,reactive->attachments,bytes);
        }
    }
    stream_job_t job = { .kind = JOB_SAVE, .coord = chunk->coord,
        .generation = generation, .chunk = copy,.colors=color_copy,.reactive=reaction_copy,.time=time };
    if(bulk) {
        job.bulk_bytes=fr_bulk_phase_chunk_bytes();job.bulk_packet=malloc(job.bulk_bytes);
        if(!job.bulk_packet||!fr_bulk_phase_chunk_encode(copy,bulk,job.bulk_packet,job.bulk_bytes)) {
            free(copy);free(color_copy);release_channel(reaction_copy);free(job.bulk_packet);return false;
        }
    }
    if(water_excess) {
        fr_thermal_checkpoint_t state={.source={0,FR_THERMAL_PROBE_CAPACITY},.chunk_count=1,.water_excess=(float *)water_excess};
        if(!fr_thermal_checkpoint_encode(&state,copy,1,&job.thermal,&job.thermal_bytes,NULL,0)) {
            free(copy);free(color_copy);release_channel(reaction_copy);free(job.bulk_packet);return false;
        }
    }
    if(gas) {
        job.gas=malloc(sizeof(*gas));
        if(!job.gas) {free(copy);free(color_copy);release_channel(reaction_copy);free(job.thermal);free(job.bulk_packet);return false;}
        memcpy(job.gas,gas,sizeof(*gas));
    }
    if (queue_job(stream, job)) return true;
    free(copy);
    free(color_copy);
    release_channel(reaction_copy);
    free(job.thermal);
    free(job.gas);
    free(job.bulk_packet);
    return false;
}

bool dc_stream_poll(dc_streamer_t *stream, dc_stream_result_t *result) {
    if (!stream || !result) return false;
    mtx_lock(&stream->mutex);
    if (!stream->result_count) { mtx_unlock(&stream->mutex); return false; }
    *result = stream->results[stream->result_head];
    memset(&stream->results[stream->result_head], 0, sizeof(*result));
    stream->result_head = (stream->result_head + 1u) % stream->capacity;
    --stream->result_count;
    cnd_signal(&stream->results_space);
    mtx_unlock(&stream->mutex);
    return true;
}

void dc_stream_result_release(dc_stream_result_t *result) {
    if (!result) return;
    free(result->chunk);
    free(result->colors);
    release_channel(result->reactive);
    free(result->water_excess);
    free(result->gas);
    free(result->bulk);
    memset(result, 0, sizeof(*result));
}

void dc_stream_destroy(dc_streamer_t *stream) {
    if (!stream) return;
    mtx_lock(&stream->mutex);
    stream->stop = true;
    cnd_broadcast(&stream->jobs_ready);
    cnd_broadcast(&stream->results_space);
    mtx_unlock(&stream->mutex);
    thrd_join(stream->thread, NULL);
    for (uint32_t i = 0; i < stream->capacity; ++i) {
        free(stream->jobs[i].chunk);
        free(stream->jobs[i].colors);
        release_channel(stream->jobs[i].reactive);
        free(stream->jobs[i].thermal);
        free(stream->jobs[i].gas);
        dc_stream_result_release(&stream->results[i]);
    }
    cnd_destroy(&stream->results_space);
    cnd_destroy(&stream->jobs_ready);
    mtx_destroy(&stream->mutex);
    free(stream->ground_surface);
    free(stream->staging);free(stream->directory); free(stream->jobs); free(stream->results); free(stream);
}
