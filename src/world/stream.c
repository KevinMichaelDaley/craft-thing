#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>

#include "dungeoncraft/generate.h"
#include "dungeoncraft/stream.h"

typedef enum { JOB_LOAD, JOB_SAVE } job_kind_t;
typedef struct {
    job_kind_t kind;
    dc_chunk_coord_t coord;
    uint64_t generation;
    dc_chunk_t *chunk;
} stream_job_t;

typedef struct {
    char magic[4];
    uint32_t version;
    int64_t x, y;
    uint64_t seed;
} chunk_header_t;

_Static_assert(sizeof(chunk_header_t) == 32, "Chunk file header layout changed");

struct dc_streamer {
    char *directory;
    uint64_t seed;
    uint32_t capacity;
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

static bool chunk_path(const dc_streamer_t *stream, dc_chunk_coord_t coord,
                       char *path, size_t cap, bool temporary) {
    int count = snprintf(path, cap, "%s/chunk_%016" PRIx64 "_%016" PRIx64 ".bin%s",
        stream->directory, (uint64_t)coord.x, (uint64_t)coord.y,
        temporary ? ".tmp" : "");
    return count > 0 && (size_t)count < cap;
}

static bool save_chunk(const dc_streamer_t *stream, const dc_chunk_t *chunk) {
    char temporary[1024], final[1024];
    if (!chunk_path(stream, chunk->coord, temporary, sizeof(temporary), true) ||
        !chunk_path(stream, chunk->coord, final, sizeof(final), false)) return false;
    FILE *file = fopen(temporary, "wb");
    if (!file) return false;
    chunk_header_t header = { .magic = {'D', 'C', 'C', '1'}, .version = 5,
        .x = chunk->coord.x, .y = chunk->coord.y, .seed = stream->seed };
    bool okay = fwrite(&header, sizeof(header), 1, file) == 1 &&
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
    if (fclose(file) != 0) okay = false;
    if (okay) okay = rename(temporary, final) == 0;
    if (!okay) remove(temporary);
    return okay;
}

static dc_chunk_t *load_chunk(const dc_streamer_t *stream, dc_chunk_coord_t coord) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return NULL;
    char path[1024];
    if (!chunk_path(stream, coord, path, sizeof(path), false)) { free(chunk); return NULL; }
    FILE *file = fopen(path, "rb");
    if (!file && errno == ENOENT) {
#ifdef DC_NATIVE_VIEW
        if (!dc_generate_chunk_scaled(stream->seed, coord, 4u, chunk)) {
            free(chunk);
            return NULL;
        }
#else
        dc_generate_chunk(stream->seed, coord, chunk);
#endif
        return chunk;
    }
    if (!file) { free(chunk); return NULL; }
    chunk_header_t header;
    bool okay = fread(&header, sizeof(header), 1, file) == 1 &&
        memcmp(header.magic, "DCC1", 4) == 0 &&
        (header.version >= 1 && header.version <= 5) &&
        header.x == coord.x && header.y == coord.y && header.seed == stream->seed &&
        fread(chunk->cells, sizeof(chunk->cells), 1, file) == 1;
    if (okay && header.version >= 2)
        okay = fread(&chunk->marker_count, sizeof(chunk->marker_count), 1, file) == 1 &&
            chunk->marker_count <= DC_MARKERS_PER_CHUNK &&
            fread(chunk->markers, sizeof(dc_marker_t), chunk->marker_count, file) ==
                chunk->marker_count;
    if (okay && header.version >= 3)
        okay = fread(chunk->face_velocity, sizeof(chunk->face_velocity), 1, file) == 1;
    if (okay && header.version >= 4) {
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
                if (header.version == 4)
                    particle.grain_fp = dc_chunk_grain_radius_fp(particle.material);
                chunk->particles[index] = particle;
            }
        }
    }
    if (fclose(file) != 0) okay = false;
    if (!okay) { free(chunk); return NULL; }
    chunk->coord = coord;
    if (header.version < 4) dc_chunk_seed_particles(chunk);
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
            result.chunk = load_chunk(stream, job.coord);
            result.kind = result.chunk ? DC_STREAM_LOADED : DC_STREAM_FAILED;
        } else {
            result.kind = save_chunk(stream, job.chunk) ? DC_STREAM_SAVED : DC_STREAM_FAILED;
            free(job.chunk);
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
    if (!directory || !directory[0] || strlen(directory) > 900 || !queue_capacity) return NULL;
    if (mkdir(directory, 0755) != 0 && errno != EEXIST) return NULL;
    dc_streamer_t *stream = calloc(1, sizeof(*stream));
    if (!stream) return NULL;
    size_t length = strlen(directory) + 1;
    stream->directory = malloc(length);
    stream->jobs = calloc(queue_capacity, sizeof(*stream->jobs));
    stream->results = calloc(queue_capacity, sizeof(*stream->results));
    if (!stream->directory || !stream->jobs || !stream->results) goto fail_alloc;
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
    free(stream->directory); free(stream->jobs); free(stream->results); free(stream);
    return NULL;
}

static bool queue_job(dc_streamer_t *stream, stream_job_t job) {
    mtx_lock(&stream->mutex);
    if (stream->stop || stream->job_count == stream->capacity) {
        mtx_unlock(&stream->mutex);
        return false;
    }
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
    if (!stream || !chunk || !generation) return false;
    dc_chunk_t *copy = malloc(sizeof(*copy));
    if (!copy) return false;
    memcpy(copy, chunk, sizeof(*copy));
    stream_job_t job = { .kind = JOB_SAVE, .coord = chunk->coord,
        .generation = generation, .chunk = copy };
    if (queue_job(stream, job)) return true;
    free(copy);
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
        dc_stream_result_release(&stream->results[i]);
    }
    cnd_destroy(&stream->results_space);
    cnd_destroy(&stream->jobs_ready);
    mtx_destroy(&stream->mutex);
    free(stream->directory); free(stream->jobs); free(stream->results); free(stream);
}
