#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>

#include "dungeoncraft/generate.h"
#include "dungeoncraft/stream.h"
#include "ferrum/fire/thermal_checkpoint.h"
#include "../fire/gas_checkpoint_internal.h"
#include "stream_packet.h"

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
            const dc_ground_profile_t *ground = stream->ground_surface ? &stream->ground : NULL;
            result.kind = dc_stream_read_packet(stream->directory, stream->seed, ground, job.staging, &result) ?
                DC_STREAM_LOADED : DC_STREAM_FAILED;
        } else {
            result.kind = dc_stream_write_packet(stream->directory, stream->seed, &job) ?
                DC_STREAM_SAVED : DC_STREAM_FAILED;
            if (result.kind == DC_STREAM_SAVED) result.replaces_owner = job.replaces_owner;
            free(job.chunk);
            free(job.colors);
            dc_stream_release_channel(job.reactive);
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
static bool request_save_snapshot(dc_streamer_t *stream,const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors,const fr_reactive_chunk_t *reactive,
    const float *water_excess,const fr_gas_checkpoint_chunk_t *gas,
    const fr_bulk_phase_chunk_t *bulk,dc_stream_chunk_time_t time,uint64_t generation,
    dc_chunk_owner_t replaces_owner) {
    fr_reactive_chunk_t empty={.next_id=1};
    if((water_excess||gas||bulk)&&!reactive) reactive=&empty;
    if (!stream || !chunk || !generation) return false;
    if (time.known && !dc_stream_valid_completed_time(time.seconds)) return false;
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
        .generation = generation, .chunk = copy,.colors=color_copy,.reactive=reaction_copy,.time=time,
        .replaces_owner = replaces_owner };
    if(bulk) {
        job.bulk_bytes=fr_bulk_phase_chunk_bytes();job.bulk_packet=malloc(job.bulk_bytes);
        if(!job.bulk_packet||!fr_bulk_phase_chunk_encode(copy,bulk,job.bulk_packet,job.bulk_bytes)) {
            free(copy);free(color_copy);dc_stream_release_channel(reaction_copy);free(job.bulk_packet);return false;
        }
    }
    if(water_excess) {
        fr_thermal_checkpoint_t state={.source={0,FR_THERMAL_PROBE_CAPACITY},.chunk_count=1,.water_excess=(float *)water_excess};
        if(!fr_thermal_checkpoint_encode(&state,copy,1,&job.thermal,&job.thermal_bytes,NULL,0)) {
            free(copy);free(color_copy);dc_stream_release_channel(reaction_copy);free(job.bulk_packet);return false;
        }
    }
    if(gas) {
        job.gas=malloc(sizeof(*gas));
        if(!job.gas) {free(copy);free(color_copy);dc_stream_release_channel(reaction_copy);free(job.thermal);free(job.bulk_packet);return false;}
        memcpy(job.gas,gas,sizeof(*gas));
    }
    if (queue_job(stream, job)) return true;
    free(copy);
    free(color_copy);
    dc_stream_release_channel(reaction_copy);
    free(job.thermal);
    free(job.gas);
    free(job.bulk_packet);
    return false;
}

bool dc_stream_request_save_snapshot(dc_streamer_t *stream,const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors,const fr_reactive_chunk_t *reactive,
    const float *water_excess,const fr_gas_checkpoint_chunk_t *gas,
    const fr_bulk_phase_chunk_t *bulk,dc_stream_chunk_time_t time,uint64_t generation) {
    return request_save_snapshot(stream, chunk, colors, reactive, water_excess, gas, bulk,
                                 time, generation, (dc_chunk_owner_t){0});
}

bool dc_stream_request_save_takeover(dc_streamer_t *stream, const dc_stream_result_t *snapshot,
    dc_chunk_owner_t replaces_owner, uint64_t write_generation) {
    if (!snapshot || snapshot->kind != DC_STREAM_LOADED || !snapshot->chunk ||
        !snapshot->generation || !snapshot->time.known || !dc_chunk_owner_valid(replaces_owner) ||
        snapshot->coord.x != snapshot->chunk->coord.x || snapshot->coord.y != snapshot->chunk->coord.y)
        return false;
    return request_save_snapshot(stream, snapshot->chunk, snapshot->colors, snapshot->reactive,
        snapshot->water_excess, snapshot->gas, snapshot->bulk, snapshot->time, write_generation,
        replaces_owner);
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
    dc_stream_release_channel(result->reactive);
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
        dc_stream_release_channel(stream->jobs[i].reactive);
        free(stream->jobs[i].thermal);
        free(stream->jobs[i].gas);
        free(stream->jobs[i].bulk_packet);
        dc_stream_result_release(&stream->results[i]);
    }
    cnd_destroy(&stream->results_space);
    cnd_destroy(&stream->jobs_ready);
    mtx_destroy(&stream->mutex);
    free(stream->ground_surface);
    free(stream->staging);free(stream->directory); free(stream->jobs); free(stream->results); free(stream);
}
