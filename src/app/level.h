#ifndef DUNGEONCRAFT_APP_LEVEL_H
#define DUNGEONCRAFT_APP_LEVEL_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"
#include "dungeoncraft/gpu.h"
#include "dungeoncraft/generate.h"
#include "ferrum/structural/gpu.h"
#include "ferrum/assets/prop.h"
#include "ferrum/renderer/gpu.h"

typedef struct dc_level_view dc_level_view_t;
/** Whether a persisted world checkpoint was restored at view creation. */
bool dc_level_view_restored(const dc_level_view_t *view);
/** Migrate corner-bond history without placing over saved scene edits. */
bool dc_level_view_restore_prop_bonds(dc_level_view_t *view,const fr_prop_asset_t *asset,
                                      dc_chunk_coord_t chunk,uint32_t x,uint32_t y,
                                      char *err,uint32_t cap);

typedef struct {
    dc_chunk_coord_t origin;
    uint32_t offset_x, offset_y;
    uint32_t ready_chunks, total_chunks;
} dc_level_view_status_t;

dc_level_view_t *dc_level_view_create(const char *directory, uint64_t seed,
                                      char *err, uint32_t err_cap);
/** Create a scene world whose missing chunks follow the authored ground line. */
dc_level_view_t *dc_level_view_create_ground(const char *directory, uint64_t seed,
                                            const dc_ground_profile_t *ground,
                                            char *err, uint32_t err_cap);
bool dc_level_view_tick(dc_level_view_t *view, char *err, uint32_t err_cap);
bool dc_level_view_step(dc_level_view_t *view, char *err, uint32_t err_cap);
/** Place terrain assets in resident world chunks, independently of visibility. */
bool dc_level_view_place_prop(dc_level_view_t *view, const fr_prop_asset_t *asset,
                              dc_chunk_coord_t chunk, uint32_t local_x, uint32_t local_y,
                              char *err, uint32_t err_cap);
/** Choose how the separately stored appearance channel is displayed. */
bool dc_level_view_renderer_mode(dc_level_view_t *view, fr_renderer_mode_t mode);
/** Toggle lit gameplay and unlit appearance without advancing physics. */
bool dc_level_view_toggle_lighting(dc_level_view_t *view);
/** Create or resume the finite heat bench in a fully resident placement world. */
bool dc_level_view_setup_heat_bench(dc_level_view_t *view,char *err,uint32_t err_cap);
/** Contact the finite probe at a visible cell; input does not advance physics. */
bool dc_level_view_heat_probe(dc_level_view_t *view,uint32_t x,uint32_t y,bool active);
/** Thermal feedback: 0 authored color, 1 temperature, 2 material wetness. */
bool dc_level_view_heat_overlay(dc_level_view_t *view,uint32_t mode);
/** Create the finite-reservoir smoke/ventilation fixture in a fresh world. */
bool dc_level_view_setup_smoke_box(dc_level_view_t *view,char *err,uint32_t err_cap);
/** Toggle fixture valve0 red,1 blue,2 enclosure vent. */
bool dc_level_view_smoke_valve(dc_level_view_t *view,uint32_t valve,char *err,uint32_t err_cap);
/** Select smoke pigment or oxygen molecular-fraction feedback. */
bool dc_level_view_smoke_overlay(dc_level_view_t *view,uint32_t mode);
/** Author explicitly placed structural members in the streamed world. */
bool dc_level_view_place_structure(dc_level_view_t *view,
                                   const fr_structural_gpu_graph_t *graph,
                                   char *err, uint32_t err_cap);
bool dc_level_view_step_timed(dc_level_view_t *view, float seconds,
                              char *err, uint32_t err_cap);
bool dc_level_view_spawn_body(dc_level_view_t *view, uint32_t x, uint32_t y,
                              char *err, uint32_t err_cap);
/** Spawn a stationary convex piece at a visible cell, allocating a stable ID. */
bool dc_level_view_spawn_convex_body(dc_level_view_t *view, uint32_t x, uint32_t y,
                                     uint32_t width, uint32_t height,
                                     const dc_gpu_body_shape_t *shape,
                                     char *err, uint32_t err_cap);
/** Opt-in completed broadphase diagnostics for streamed-window tests. */
bool dc_level_view_broadphase(dc_level_view_t *view, dc_gpu_broadphase_stats_t *stats,
                              dc_gpu_broadphase_pair_t *pairs, uint32_t pair_cap,
                              char *err, uint32_t err_cap);
/** Read a stable body's world pose and physical state for window diagnostics. */
bool dc_level_view_body_state(dc_level_view_t *view, uint32_t id,
                              dc_gpu_world_body_t *body, dc_gpu_body_motion_t *motion,
                              char *err, uint32_t err_cap);
/** Read solver configuration and completed tick counters for window diagnostics. */
bool dc_level_view_rigid_solver_stats(dc_level_view_t *view, dc_gpu_rigid_solver_stats_t *stats);
/** Opt-in completed narrowphase diagnostics for streamed-window tests. */
bool dc_level_view_contacts(dc_level_view_t *view, dc_gpu_contact_stats_t *stats,
                            dc_gpu_contact_t *contacts, uint32_t contact_cap,
                            char *err, uint32_t err_cap);
bool dc_level_view_wait_visible(dc_level_view_t *view, uint32_t timeout_ms,
                                char *err, uint32_t err_cap);
bool dc_level_view_move(dc_level_view_t *view, int32_t dx, int32_t dy);
bool dc_level_view_pan_pixels(dc_level_view_t *view, int32_t dx, int32_t dy);
bool dc_level_view_reset_camera(dc_level_view_t *view);
bool dc_level_view_status(dc_level_view_t *view, dc_level_view_status_t *status);
bool dc_level_view_set_title(dc_level_view_t *view, const char *title);
bool dc_level_view_toggle_marker_overlay(dc_level_view_t *view);
bool dc_level_view_set_zoom(dc_level_view_t *view, uint32_t zoom);
bool dc_level_view_screen_cell(dc_level_view_t *view, uint32_t screen_x,
                               uint32_t screen_y, uint32_t *cell_x,
                               uint32_t *cell_y);
bool dc_level_view_set_overlay(dc_level_view_t *view, dc_gpu_overlay_t overlay);
bool dc_level_view_set_spring_enabled(dc_level_view_t *view, bool enabled);
bool dc_level_view_set_marker_correction(dc_level_view_t *view, bool enabled);
bool dc_level_view_set_fluid_interval(dc_level_view_t *view, uint32_t interval);
bool dc_level_view_paint(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t radius, uint16_t material,
                         char *err, uint32_t err_cap);
bool dc_level_view_pixel(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t *color, char *err, uint32_t err_cap);
bool dc_level_view_pixels(dc_level_view_t *view, uint32_t *colors,
                          uint32_t count, char *err, uint32_t err_cap);
/** Capture one complete GPU physics tick for opt-in profiling. */
bool dc_level_view_capture_tick(dc_level_view_t *view,
                                dc_gpu_tick_capture_t *capture,
                                char *err, uint32_t err_cap);
bool dc_level_view_memory_stats(dc_level_view_t *view,
                                dc_gpu_memory_stats_t *stats);
bool dc_level_view_chunk(dc_level_view_t *view, dc_chunk_coord_t coord,
                         dc_chunk_t *chunk, char *err, uint32_t err_cap);
bool dc_level_view_has_chunk(dc_level_view_t *view, dc_chunk_coord_t coord);
/** Queue an adjacent world-cell transfer across streamed chunks. */
bool dc_level_view_queue_transfer(dc_level_view_t *view, int64_t from_x, int64_t from_y,
                                  int64_t to_x, int64_t to_y, uint32_t amount,
                                  dc_gpu_transfer_kind_t kind, char *err, uint32_t cap);
/** Return true once the queued transfer was applied or rejected by cell contents. */
bool dc_level_view_transfer_result(dc_level_view_t *view,
                                   dc_gpu_transfer_state_t *state);
bool dc_level_view_destroy(dc_level_view_t *view, char *err, uint32_t err_cap);

#endif
