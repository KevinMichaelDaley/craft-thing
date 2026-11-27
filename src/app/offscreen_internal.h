#ifndef DC_APP_OFFSCREEN_INTERNAL_H
#define DC_APP_OFFSCREEN_INTERNAL_H

#include "level_offscreen.h"

enum { OFFSCREEN_SIDE = 3u, OFFSCREEN_SLOTS = OFFSCREEN_SIDE * OFFSCREEN_SIDE };

enum { DC_OFFSCREEN_BOND_CAPACITY = 192u };
typedef struct {
    uint32_t slot, id, member_generation;
} dc_offscreen_bond_member_t;

typedef struct {
    dc_offscreen_bond_member_t members[2u * DC_OFFSCREEN_BOND_CAPACITY];
    dc_chunk_coord_t pages[OFFSCREEN_SLOTS];
    uint32_t member_count, page_count;
} dc_offscreen_bond_group_t;

typedef struct dc_camera_bond {
    dc_offscreen_bond_group_t group;
    struct {
        dc_chunk_owner_t owner;
        uint64_t save_generation, lease_generation;
        bool saved, returned;
    } receipts[OFFSCREEN_SLOTS];
    unsigned occupied_pages;
    bool owns, returning;
} dc_camera_bond_t;

/** Identify the complete converted assembly occupying the offered page. */
bool dc_offscreen_bond_group_at(dc_gpu_t *gpu, dc_chunk_coord_t page, dc_offscreen_bond_group_t *group);
/** Check exterior walls while allowing internal faces between group pages. */
bool dc_camera_bond_page_sealed(const dc_offscreen_bond_group_t *group, const dc_stream_result_t *source);
/** Extend the footprint through open interior faces to the walls of the workspace. */
bool dc_camera_bond_enclose(dc_level_view_t *view, dc_offscreen_bond_group_t *group);
bool dc_offscreen_bond_add_page(dc_offscreen_bond_group_t *group, dc_chunk_coord_t page);
bool dc_offscreen_bond_pages_clear(dc_gpu_t *gpu, const dc_offscreen_bond_group_t *group);
bool dc_camera_bond_offer(dc_level_view_t *view, const dc_stream_result_t *source,
    dc_chunk_owner_t owner, uint64_t generation);
void dc_camera_bond_saved(dc_level_view_t *view, const dc_stream_result_t *result);
bool dc_camera_bond_service(dc_level_view_t *view, bool closing, char *err, uint32_t cap);
void dc_offscreen_release_workspace_leases(dc_level_view_t *view, dc_offscreen_t *workspace);

/** Collect a live connected assembly and the pages touched by its current geometry. */
bool dc_offscreen_collect_bond_group(dc_gpu_t *gpu, uint32_t id, dc_offscreen_bond_group_t *group);
/** Move a confirmed converted group, preserving member generations and remapping bond slots. */
bool dc_offscreen_move_bond_group(dc_gpu_t *from, dc_gpu_t *to, const dc_offscreen_bond_group_t *group,
    char *err, uint32_t cap);
/** Partition the graph and its history. A NULL destination checks eligibility before camera election. */
bool dc_offscreen_partition_graph(dc_gpu_t *from, dc_gpu_t *to, const dc_offscreen_bond_group_t *group,
    const uint32_t *dest_slots);

enum { OFFSCREEN_STEP_LIMIT = 16u };

struct dc_offscreen {
    dc_gpu_t *gpu;
    dc_chunk_coord_t origin;
    uint64_t generations[OFFSCREEN_SLOTS];
    dc_chunk_time_t time;
    double due_seconds;
    /* Each entry retains the target used to compute one native step. Only
       time is confirmed; replay_end is the endpoint after all queued steps. */
    double replay_targets[OFFSCREEN_STEP_LIMIT], replay_end;
    unsigned replay_count;
    uint32_t held_writes, references;
    bool failed, water_coupled;
};

/** Plan native elapsed steps without advancing the confirmed solver clock. */
bool dc_offscreen_plan_replay(dc_offscreen_t *room, unsigned budget, char *err, uint32_t cap);
/** Run queued native steps and remove each round only after its fence succeeds. */
bool dc_offscreen_run_replay_group(dc_offscreen_t *const *rooms, unsigned count, unsigned budget,
                                   char *err, uint32_t cap);
/** Complete already planned steps before changing ownership or simulation paths. */
bool dc_offscreen_drain_replay(dc_offscreen_t *room, char *err, uint32_t cap);

/** Advance exclusively leased, independently due rooms and commit their clocks after each fence. */
bool dc_offscreen_advance_due_group(dc_offscreen_t *const *rooms, unsigned count, unsigned budget,
                                    char *err, uint32_t cap);

typedef struct dc_level_offscreen_handoff {
    struct dc_level_offscreen_handoff *next;
    dc_offscreen_t *workspace;
    dc_chunk_coord_t coord;
    dc_chunk_owner_t owner;
    dc_chunk_time_t source_time;
    uint64_t generation, write_generation;
    double liquid_face_end[4];
    bool admitted, disk_source, liquid_paired, owns_bodies;
} dc_level_offscreen_handoff_t;

dc_level_offscreen_result_t dc_camera_bond_result(dc_level_view_t *view, dc_level_offscreen_handoff_t *lease,
    const dc_stream_result_t *result, char *err, uint32_t cap);

/** Next native elapsed step, rounded down to the remaining confirmed interval. */
float dc_offscreen_step_seconds(double remaining);

/** Resolve a world coordinate to the bounded workspace atlas. */
bool dc_offscreen_page_at(const dc_offscreen_t *workspace, dc_chunk_coord_t coord, uint32_t *slot);
/** Check that the sealed room contains a complete independently movable body group. */
bool dc_offscreen_body_island(dc_gpu_t *gpu, dc_chunk_coord_t coord);
/** Publish a complete body group between two idle owners after checking destination capacity. */
bool dc_offscreen_move_bodies(dc_gpu_t *from, dc_gpu_t *to, dc_chunk_coord_t coord, char *err, uint32_t cap);
/** Borrow the current lease for a coordinate during main-thread work. */
dc_level_offscreen_handoff_t *dc_level_offscreen_lookup(dc_level_view_t *view, dc_chunk_coord_t coord);

#endif
