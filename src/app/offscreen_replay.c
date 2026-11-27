#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../vulkan/gpu_internal.h"
#include "offscreen_internal.h"

static const double OFFSCREEN_MAX_STEP_SECONDS = .05;

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

float dc_offscreen_step_seconds(double remaining) {
    const double interval = fmin(remaining, OFFSCREEN_MAX_STEP_SECONDS);
    const float seconds = (float)interval;
    return (double)seconds > interval ? nextafterf(seconds, 0.f) : seconds;
}

static bool prepare_step(dc_offscreen_t *workspace, double goal, double *completed, char *err, uint32_t cap) {
    const float seconds = dc_offscreen_step_seconds(goal - workspace->time.seconds);
    *completed = workspace->time.seconds + (double)seconds;
    if (*completed <= workspace->time.seconds || !dc_gpu_set_tick_seconds(workspace->gpu, seconds))
        return error(err, cap, "Offscreen interval cannot advance the confirmed clock");
    return true;
}

static bool advance_group(dc_offscreen_t *const *rooms, const double *targets, unsigned count, unsigned budget,
                                    char *err, uint32_t cap) {
    if (!count) return true;
    if (count > DC_GPU_TICK_BATCH_LIMIT || budget > OFFSCREEN_STEP_LIMIT)
        return error(err, cap, "Invalid offscreen replay group size");
    for (unsigned step = 0; step < budget; ++step) {
        dc_gpu_t *owners[DC_GPU_TICK_BATCH_LIMIT];
        dc_offscreen_t *active[DC_GPU_TICK_BATCH_LIMIT];
        double completed[DC_GPU_TICK_BATCH_LIMIT];
        unsigned due = 0;
        for (unsigned i = 0; i < count; ++i) {
            dc_offscreen_t *room = rooms[i];
            const double target = targets ? targets[i] : room->due_seconds;
            if (room->time.seconds >= target) continue;
            if (!prepare_step(room, target, &completed[due], err, cap)) {
                room->failed = true;
                return false;
            }
            owners[due] = room->gpu;
            active[due++] = room;
        }
        if (!due) break;
        if (!dc_gpu_tick_independent_batch(owners, due, err, cap)) {
            for (unsigned i = 0; i < due; ++i) active[i]->failed = true;
            return false;
        }
        for (unsigned i = 0; i < due; ++i) active[i]->time.seconds = completed[i];
    }
    return true;
}

bool dc_offscreen_advance(dc_offscreen_t *workspace, double world_seconds, uint32_t step_budget,
                          char *err, uint32_t cap) {
    if (!workspace || workspace->failed || !isfinite(world_seconds) || world_seconds < 0.)
        return error(err, cap, "Invalid offscreen advance or failed workspace");
    if (!workspace->time.known || workspace->held_writes || workspace->water_coupled ||
        dc_gpu_liquid_owner_held(workspace->gpu))
        return true;
    if (step_budget > OFFSCREEN_STEP_LIMIT) step_budget = OFFSCREEN_STEP_LIMIT;
    for (uint32_t i = 0; i < step_budget && workspace->time.seconds < world_seconds; ++i) {
        double completed;
        if (!prepare_step(workspace, world_seconds, &completed, err, cap)) return false;
        if (!dc_gpu_tick_step(workspace->gpu, err, cap)) {
            /* Submission may already have changed one or more physics channels. */
            workspace->failed = true;
            return false;
        }
        workspace->time.seconds = completed;
    }
    return true;
}

bool dc_offscreen_advance_due_group(dc_offscreen_t *const *rooms, unsigned count, unsigned budget,
                                    char *err, uint32_t cap) {
    return advance_group(rooms, NULL, count, budget, err, cap);
}

bool dc_offscreen_plan_replay(dc_offscreen_t *room, unsigned budget, char *err, uint32_t cap) {
    unsigned count = room->replay_count;
    double end = count ? room->replay_end : room->time.seconds;
    for (unsigned i = 0; i < budget && end < room->due_seconds; ++i) {
        const double next = end + dc_offscreen_step_seconds(room->due_seconds - end);
        if (next <= end || count == OFFSCREEN_STEP_LIMIT)
            return error(err, cap, "Offscreen replay plan cannot advance");
        room->replay_targets[count++] = room->due_seconds;
        end = next;
    }
    room->replay_end = end;
    room->replay_count = count;
    return true;
}

static void consume_step(dc_offscreen_t *room) {
    --room->replay_count;
    memmove(room->replay_targets, room->replay_targets + 1, room->replay_count * sizeof(double));
}

bool dc_offscreen_run_replay_group(dc_offscreen_t *const *rooms, unsigned count, unsigned budget,
                                   char *err, uint32_t cap) {
    if (count > DC_GPU_TICK_BATCH_LIMIT) return error(err, cap, "Too many camera replay owners");
    for (unsigned round = 0; round < budget; ++round) {
        dc_offscreen_t *active[DC_GPU_TICK_BATCH_LIMIT];
        double targets[DC_GPU_TICK_BATCH_LIMIT];
        unsigned due = 0;
        for (unsigned i = 0; i < count; ++i) {
            if (!rooms[i]->replay_count) continue;
            active[due] = rooms[i];
            targets[due++] = rooms[i]->replay_targets[0];
        }
        if (!due) break;
        if (!advance_group(active, targets, due, 1, err, cap)) return false;
        for (unsigned i = 0; i < due; ++i) consume_step(active[i]);
    }
    return true;
}

bool dc_offscreen_drain_replay(dc_offscreen_t *room, char *err, uint32_t cap) {
    while (room->replay_count) {
        const double start = room->time.seconds;
        if (!dc_offscreen_advance(room, room->replay_targets[0], 1, err, cap)) return false;
        if (room->time.seconds <= start) return error(err, cap, "Cannot drain a held camera replay");
        consume_step(room);
    }
    return true;
}
