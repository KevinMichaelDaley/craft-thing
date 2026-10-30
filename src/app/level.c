#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "dungeoncraft/gpu.h"
#include "dungeoncraft/stream.h"
#include "ferrum/assets/prop_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/fire/combustion_gpu.h"
#include "bonfire.h"
#include "level.h"
#include "level_internal.h"
#include "thermal_bench.h"
#include "view_config.h"

static bool error(char *buf,uint32_t cap,const char *message);
static void bonfire_startup_trace(const char *stage, Uint64 started) {
    if(getenv("DC_BONFIRE_STARTUP_TRACE"))
        fprintf(stderr,"Bonfire startup %s: %llums\n",stage,
                (unsigned long long)(SDL_GetTicks64()-started));
}
bool dc_level_view_setup_bonfire(dc_level_view_t *view,char *err,uint32_t cap) {
    Uint64 started=SDL_GetTicks64();
    if(!view) return error(err,cap,"Missing bonfire level view");
    if(!((VIEW_WIDTH==480&&VIEW_HEIGHT==270)||
         (VIEW_WIDTH==960&&VIEW_HEIGHT==540)))
        return error(err,cap,"Bonfire view requires quarter or half native resolution");
    if(view->restored) {
        if(!fr_thermal_gpu_is_bonfire_world(view->gpu)) return error(err,cap,"Saved world is not an ordinary bonfire");
    } else if(!fr_thermal_gpu_is_bonfire_world(view->gpu)) {
        if(!dc_level_view_wait_visible(view,120000,err,cap)) return false;
        bonfire_startup_trace("visible chunks",started);
        uint32_t count=view->table.capacity;
        dc_chunk_coord_t *coords=calloc(count,sizeof(*coords));
        dc_thermal_bench_chunk_t *chunks=calloc(count,sizeof(*chunks));
        if(!coords||!chunks) {free(coords);free(chunks);return error(err,cap,"Cannot preflight bonfire fixture");}
        for(uint32_t i=0;i<count;i++) coords[i]=view->table.slots[i].coord;
        bool okay=dc_bonfire_build(coords,count,chunks,err,cap);free(coords);
        bonfire_startup_trace("fixture built",started);
        Uint64 upload_ms=0,color_ms=0,reactive_ms=0,reserve_ms=0;
        uint64_t reactive_count=0;
        for(uint32_t i=0;i<count&&okay;i++) {
            Uint64 stage=SDL_GetTicks64();
            okay=dc_gpu_upload_chunk(view->gpu,i,chunks[i].chunk,err,cap);
            upload_ms+=SDL_GetTicks64()-stage;
            if(!okay) break;
            stage=SDL_GetTicks64();
            okay=fr_renderer_upload_chunk_colors(view->gpu,i,chunks[i].colors,err,cap);
            color_ms+=SDL_GetTicks64()-stage;
            reactive_count+=chunks[i].reactive.count;
        }
        if(okay&&reactive_count>UINT32_MAX) okay=error(err,cap,"Bonfire reactive import exceeds address range");
        if(okay&&reactive_count) {
            Uint64 stage=SDL_GetTicks64();
            okay=fr_gpu_reactive_reserve_import(view->gpu,(uint32_t)reactive_count,err,cap);
            reserve_ms=SDL_GetTicks64()-stage;
        }
        for(uint32_t i=0;i<count&&okay;i++) {
            Uint64 stage=SDL_GetTicks64();
            okay=fr_gpu_reactive_import_chunk(view->gpu,i,chunks[i].chunk,&chunks[i].reactive,err,cap)&&
                dc_chunk_table_mark_dirty(&view->table,i);
            reactive_ms+=SDL_GetTicks64()-stage;
        }
        if(getenv("DC_BONFIRE_STARTUP_TRACE"))
            fprintf(stderr,"Bonfire fixture upload chunks%u terrain%llums colors%llums reserve%llums reactive%llums\n",
                    count,(unsigned long long)upload_ms,(unsigned long long)color_ms,
                    (unsigned long long)reserve_ms,(unsigned long long)reactive_ms);
        dc_thermal_bench_release(chunks,count);
        bonfire_startup_trace("fixture uploaded",started);
        if(!okay||!fr_thermal_gpu_enable(view->gpu,err,cap)||!fr_thermal_gpu_mark_bonfire_world(view->gpu)) return false;
        if(!dc_bonfire_admit(view->gpu,err,cap)) return false;
        bonfire_startup_trace("rigid admission",started);
    }
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    if(!fr_gas_gpu_enable(view->gpu,&model,.0012f,293.15f,err,cap)) return false;
    bonfire_startup_trace("gas enabled",started);
    if(!fr_gas_gpu_set_pressure_policy(view->gpu,FR_GAS_PRESSURE_PRACTICAL,err,cap)||
       !fr_gas_gpu_set_gravity(view->gpu,900,err,cap)||!fr_combustion_gpu_enable(view->gpu,err,cap)) return false;
    if(view->restored) {
        if(!dc_level_view_wait_visible(view,120000,err,cap)) return false;
        for(uint32_t i=0;i<view->table.capacity;i++) if(!view->gas_admitted[i])
            return error(err,cap,"Saved bonfire is missing authoritative gas stock");
    } else {
        for(uint32_t i=0;i<view->table.capacity;i++) if(!view->gas_admitted[i]) {
            if(!fr_gas_gpu_queue_ambient_chunk(view->gpu,i,err,cap)) return false;
            view->gas_admitted[i]=true;
        }
        if(!fr_gas_gpu_initialize_queued(view->gpu,err,cap)) return false;
    }
    view->smoke_box=view->smoke_ready=true;
    return dc_level_view_set_spring_enabled(view,false)&&fr_thermal_gpu_overlay(view->gpu,0)&&
        dc_level_view_renderer_mode(view,FR_RENDERER_ALBEDO);
}
dc_level_view_t *dc_level_view_create_bonfire(const char *directory,uint64_t seed,char *err,uint32_t cap) {
    const uint32_t surface=96;dc_ground_profile_t ground={.width=1,.soil_depth=1,.surface=&surface};
    dc_level_view_t *view=dc_level_view_create_ground(directory,seed,&ground,err,cap);
    if(view&&dc_level_view_setup_bonfire(view,err,cap)) return view;
    if(view) {dc_stream_destroy(view->stream);dc_gpu_destroy(view->gpu);dc_chunk_table_destroy(&view->table);free(view);}
    return NULL;
}

_Static_assert(DC_GPU_CHUNK_SLOTS >= SIM_CHUNKS_X * SIM_CHUNKS_Y,
               "GPU chunk pool must cover viewport and halo");

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}
bool dc_level_view_restored(const dc_level_view_t *view) {return view&&view->restored;}

bool dc_level_view_step(dc_level_view_t *view, char *err, uint32_t cap) {
    return dc_level_view_step_timed(view, 1.0f / 60.0f, err, cap);
}
bool dc_level_view_renderer_mode(dc_level_view_t *view, fr_renderer_mode_t mode) {
    char err[256];
    if (!view || !fr_renderer_enable(view->gpu,err,sizeof(err))) return false;
    if (!view->lighting_initialized) {
        fr_renderer_sun_t sun={.direction={-.4f,-.8f,.45f},.intensity=1.8f,
            .color={1.f,.95f,.85f},.known_sky=FR_SKY_TOP|FR_SKY_LEFT};
        if (!fr_renderer_set_sources(view->gpu,NULL,0,sun)) return false;
        view->lighting_initialized=true;
    }
    if (!fr_renderer_set_mode(view->gpu,mode)) return false;
    view->renderer_mode=mode;return true;
}
bool dc_level_view_toggle_lighting(dc_level_view_t *view) {
    return view && dc_level_view_renderer_mode(view,
        view->renderer_mode==FR_RENDERER_LIT?FR_RENDERER_ALBEDO:FR_RENDERER_LIT);
}
bool dc_level_view_setup_heat_bench(dc_level_view_t *view,char *err,uint32_t cap) {
    if(!view||VIEW_WIDTH!=480||VIEW_HEIGHT!=270)
        return error(err,cap,"Heat bench requires quarter-native placement resolution");
    if(view->restored) {
        if(!fr_thermal_gpu_is_bench_world(view->gpu)) return error(err,cap,"Saved world is not an authored heat bench");
        return dc_level_view_wait_visible(view,120000,err,cap)&&dc_level_view_set_spring_enabled(view,false)&&
            dc_level_view_renderer_mode(view,FR_RENDERER_ALBEDO);
    }
    if(view->origin.x!=0||view->origin.y!=INITIAL_CHUNK_Y) return error(err,cap,"Fresh heat bench requires its initial world origin");
    if(!dc_level_view_wait_visible(view,120000,err,cap)) return false;
    uint32_t count=view->table.capacity;
    dc_chunk_coord_t *coords=calloc(count,sizeof(*coords));
    dc_thermal_bench_chunk_t *chunks=calloc(count,sizeof(*chunks));
    if(!coords||!chunks) {free(coords);free(chunks);return error(err,cap,"Cannot preflight heat bench scene");}
    for(uint32_t i=0;i<count;i++) coords[i]=view->table.slots[i].coord;
    bool okay=dc_thermal_bench_build(coords,count,chunks,err,cap);free(coords);
    for(uint32_t i=0;okay&&i<count;i++) {
        okay=dc_gpu_upload_chunk(view->gpu,i,chunks[i].chunk,err,cap)&&
            fr_renderer_upload_chunk_colors(view->gpu,i,chunks[i].colors,err,cap)&&
            fr_gpu_reactive_import_chunk(view->gpu,i,chunks[i].chunk,&chunks[i].reactive,err,cap)&&
            dc_chunk_table_mark_dirty(&view->table,i);
    }
    dc_thermal_bench_release(chunks,count);
    return okay&&dc_level_view_set_spring_enabled(view,false)&&
        dc_level_view_renderer_mode(view,FR_RENDERER_ALBEDO)&&fr_thermal_gpu_enable(view->gpu,err,cap)&&
        fr_thermal_gpu_mark_bench_world(view->gpu);
}
bool dc_level_view_heat_probe(dc_level_view_t *view,uint32_t x,uint32_t y,bool active) {
    return view&&x<VIEW_WIDTH&&y<VIEW_HEIGHT&&fr_thermal_gpu_probe(view->gpu,
        x+DC_CHUNK_SIDE+view->camera_offset_x,y+DC_CHUNK_SIDE+view->camera_offset_y,active);
}
bool dc_level_view_heat_overlay(dc_level_view_t *view,uint32_t mode) {
    return view&&fr_thermal_gpu_overlay(view->gpu,mode);
}
static bool place_prop(dc_level_view_t *view, const fr_prop_asset_t *asset,
                              dc_chunk_coord_t chunk, uint32_t local_x, uint32_t local_y,
                              bool restore_bonds,char *err, uint32_t cap) {
    if (!view || !asset || local_x>=DC_CHUNK_SIDE || local_y>=DC_CHUNK_SIDE)
        return error(err,cap,"Invalid prop world address");
    long double x=((long double)chunk.x-view->mapped_origin.x+HALO_CHUNKS)*64+local_x;
    long double y=((long double)chunk.y-view->mapped_origin.y+HALO_CHUNKS)*64+local_y;
    if (x<0 || y<0 || x+asset->width>SIM_WIDTH || y+asset->height>SIM_HEIGHT)
        return error(err,cap,"Prop destination is not resident");
    bool okay=restore_bonds?fr_gpu_restore_terrain_prop_bonds(view->gpu,asset,0,(uint32_t)x,(uint32_t)y,err,cap):
        fr_gpu_place_terrain_prop(view->gpu,asset,0,(uint32_t)x,(uint32_t)y,err,cap);
    if(!okay) return false;
    for (uint32_t i=0;i<view->table.capacity;i++)
        if (view->table.slots[i].state==DC_SLOT_ACTIVE || view->table.slots[i].state==DC_SLOT_SLEEPING)
            dc_chunk_table_mark_dirty(&view->table,i);
    return true;
}
bool dc_level_view_place_prop(dc_level_view_t *view,const fr_prop_asset_t *asset,
                              dc_chunk_coord_t chunk,uint32_t x,uint32_t y,char *err,uint32_t cap) {
    return place_prop(view,asset,chunk,x,y,false,err,cap);
}
bool dc_level_view_restore_prop_bonds(dc_level_view_t *view,const fr_prop_asset_t *asset,
                                      dc_chunk_coord_t chunk,uint32_t x,uint32_t y,char *err,uint32_t cap) {
    return place_prop(view,asset,chunk,x,y,true,err,cap);
}

bool dc_level_view_place_structure(dc_level_view_t *view,
                                   const fr_structural_gpu_graph_t *graph,
                                   char *err, uint32_t cap) {
    if (!view || !fr_gpu_place_structure(view->gpu, graph, err, cap)) return false;
    for (unsigned i = 0; i < view->table.capacity; i++)
        if (view->table.slots[i].state == DC_SLOT_ACTIVE ||
            view->table.slots[i].state == DC_SLOT_SLEEPING)
            dc_chunk_table_mark_dirty(&view->table, i);
    return fr_gpu_save_structure(view->gpu, view->body_path, err, cap);
}

bool dc_level_view_step_timed(dc_level_view_t *view, float seconds,
                              char *err, uint32_t cap) {
    if (!view) return error(err, cap, "Level view is null");
    if (view->pending_steps == UINT32_MAX)
        return error(err, cap, "Too many pending level steps");
    ++view->pending_steps;
    view->pending_seconds += seconds;
    for (uint32_t i = 0; i < view->table.capacity; ++i)
        if (view->table.slots[i].state == DC_SLOT_ACTIVE)
            dc_chunk_table_mark_dirty(&view->table, i);
    return true;
}

bool dc_level_view_spawn_body(dc_level_view_t *view, uint32_t x, uint32_t y,
                              char *err, uint32_t cap) {
    if (!view || x > VIEW_WIDTH - 4 || y > VIEW_HEIGHT - 4)
        return error(err, cap, "Invalid rigid body spawn position");
    if (!dc_gpu_set_body_origin(view->gpu, (dc_chunk_coord_t){
            view->origin.x - HALO_CHUNKS, view->origin.y - HALO_CHUNKS }, err, cap)) return false;
    dc_gpu_body_t body = { .x_fp = (int32_t)(x + DC_CHUNK_SIDE +
                                           view->camera_offset_x) << 16,
        .y_fp = (int32_t)(y + DC_CHUNK_SIDE +
                          view->camera_offset_y) << 16, .vx_fp = 1 << 16,
        .width = 4, .height = 4, .id = dc_gpu_next_body_id(view->gpu), .active = 1 };
    return dc_gpu_spawn_body(view->gpu, body, err, cap);
}

bool dc_level_view_spawn_convex_body(dc_level_view_t *view, uint32_t x, uint32_t y,
                                     uint32_t width, uint32_t height,
                                     const dc_gpu_body_shape_t *shape,
                                     char *err, uint32_t cap) {
    if (!view || !width || !height || width > DC_GPU_BODY_MAX_SIDE || height > DC_GPU_BODY_MAX_SIDE ||
        x > VIEW_WIDTH - width || y > VIEW_HEIGHT - height)
        return error(err, cap, "Invalid convex spawn position");
    if (!dc_gpu_set_body_origin(view->gpu, (dc_chunk_coord_t){
            view->origin.x - HALO_CHUNKS, view->origin.y - HALO_CHUNKS }, err, cap)) return false;
    uint32_t local_x = x + view->camera_offset_x, local_y = y + view->camera_offset_y;
    dc_gpu_world_body_t body = { .chunk = {
        view->origin.x + local_x / DC_CHUNK_SIDE,
        view->origin.y + local_y / DC_CHUNK_SIDE },
        .body = { .x_fp = (int32_t)(local_x % DC_CHUNK_SIDE) << 16,
            .y_fp = (int32_t)(local_y % DC_CHUNK_SIDE) << 16,
            .width = width, .height = height,
            .id = dc_gpu_next_body_id(view->gpu), .active = 1 } };
    return dc_gpu_spawn_convex_body(view->gpu, body, shape, err, cap);
}

bool dc_level_view_broadphase(dc_level_view_t *view, dc_gpu_broadphase_stats_t *stats,
                              dc_gpu_broadphase_pair_t *pairs, uint32_t pair_cap,
                              char *err, uint32_t cap) {
    return view && dc_gpu_read_broadphase(view->gpu, stats, pairs, pair_cap, err, cap);
}

bool dc_level_view_contacts(dc_level_view_t *view, dc_gpu_contact_stats_t *stats,
                            dc_gpu_contact_t *contacts, uint32_t contact_cap,
                            char *err, uint32_t cap) {
    return view && dc_gpu_read_contacts(view->gpu, stats, contacts, contact_cap, err, cap);
}

static bool visible(const dc_level_view_t *view, dc_chunk_coord_t coord) {
    return coord.x >= view->origin.x - HALO_CHUNKS &&
           coord.x < view->origin.x + VIEW_CHUNKS_X + HALO_CHUNKS &&
           coord.y >= view->origin.y - HALO_CHUNKS &&
           coord.y < view->origin.y + VIEW_CHUNKS_Y + HALO_CHUNKS;
}

static bool process_results(dc_level_view_t *view, char *err, uint32_t cap) {
    dc_stream_result_t result;
    while (dc_stream_poll(view->stream, &result)) {
        uint32_t index;
        if (!dc_chunk_table_find(&view->table, result.coord, &index) ||
            view->table.slots[index].generation != result.generation) {
            dc_stream_result_release(&result);
            continue;
        }
        if (result.kind == DC_STREAM_FAILED) {
            dc_stream_result_release(&result);
            return error(err, cap, "Chunk worker load or save failed");
        }
        if (result.kind == DC_STREAM_LOADED) {
            bool restored = false;
            for (uint32_t i=0;i<DC_CHUNK_CELLS;i++) {
                uint32_t m=result.chunk->cells[i].material;
                if (m==DC_MATERIAL_STONE || m==DC_MATERIAL_WOOD || m==DC_MATERIAL_DENSE_WOOD || m==DC_MATERIAL_LEAVES)
                    result.chunk->cells[i].flags |= DC_CELL_LOADED_TERRAIN;
            }
            if (!fr_gpu_upload_structure_chunk(view->gpu,index,result.chunk,&restored,err,cap)) {
                dc_stream_result_release(&result);
                return error(err, cap, "Cannot publish loaded GPU chunk");
            }
            if(!restored&&((result.colors&&!fr_renderer_upload_chunk_colors(view->gpu,index,result.colors,err,cap))||
                !fr_gpu_reactive_import_chunk(view->gpu,index,result.chunk,result.reactive,err,cap)||
                (result.water_excess&&!fr_thermal_gpu_import_water_chunk(view->gpu,index,result.chunk,result.water_excess,err,cap)))) {
                dc_stream_result_release(&result);return false;
            }
            if(result.gas&&!fr_gas_gpu_import_checkpoint(view->gpu,index,result.gas,err,cap)) {
                dc_stream_result_release(&result);return false;
            }
            view->gas_admitted[index]=result.gas!=NULL;
            if(!result.gas&&view->smoke_ready) {
                if(!fr_gas_gpu_queue_ambient_chunk(view->gpu,index,err,cap)) {dc_stream_result_release(&result);return false;}
                view->gas_admitted[index]=true;
            }
            if(!dc_chunk_table_finish_load(&view->table,index,result.generation)) {
                dc_stream_result_release(&result);return error(err,cap,"Cannot finish loaded chunk publication");
            }
            if(restored||(!result.gas&&view->smoke_ready)) dc_chunk_table_mark_dirty(&view->table,index);
        } else if (!dc_chunk_table_finish_save(&view->table, index, result.generation)) {
            dc_stream_result_release(&result);
            return error(err, cap, "Cannot complete chunk save");
        }
        dc_stream_result_release(&result);
    }
    return true;
}

static bool schedule_saves(dc_level_view_t *view, char *err, uint32_t cap) {
    if (fr_gpu_structure_checkpoint_required(view->gpu)) {
        bool pending = false;
        for (unsigned i = 0; i < view->table.capacity; i++) {
            const dc_chunk_slot_t *slot = &view->table.slots[i];
            pending |= slot->state == DC_SLOT_SLEEPING && slot->dirty && !slot->pinned;
        }
        if (pending && !fr_gpu_save_structure(view->gpu, view->body_path, err, cap)) return false;
    }
    for (uint32_t i = 0; i < view->table.capacity; ++i) {
        dc_chunk_slot_t *slot = &view->table.slots[i];
        if (slot->state != DC_SLOT_SLEEPING || !slot->dirty || slot->pinned) continue;
        dc_chunk_t chunk = { .coord = slot->coord };
        dc_chunk_colors_t colors;
        uint64_t generation;
        if (!dc_gpu_download_chunk(view->gpu, i, &chunk, err, cap)) return false;
        if(!fr_renderer_download_chunk_colors(view->gpu,i,&colors,err,cap)) return false;
        fr_reactive_chunk_t reactive={0};
        if(!fr_gpu_reactive_export_chunk(view->gpu,i,&chunk,&reactive,err,cap)) return false;
        const fr_reactive_chunk_t *channel=reactive.count||reactive.next_id>1u?&reactive:NULL;
        float water_excess[4096];const float *liquid=NULL;
        if(fr_thermal_gpu_enabled(view->gpu)) {
            if(!fr_thermal_gpu_read_water_chunk(view->gpu,i,water_excess,4096,err,cap)) {
                fr_reactive_chunk_release(&reactive);return false;
            }
            liquid=water_excess;
        }
        fr_gas_checkpoint_chunk_t *gas=NULL;
        if(view->smoke_box) {
            gas=malloc(sizeof(*gas));
            if(!gas||!fr_gas_gpu_export_checkpoint(view->gpu,i,gas,err,cap)) {
                free(gas);fr_reactive_chunk_release(&reactive);return false;
            }
        }
        if (!dc_chunk_table_begin_save(&view->table, i, &generation)) {
            free(gas);fr_reactive_chunk_release(&reactive);
            return error(err, cap, "Cannot begin dirty chunk save");
        }
        bool queued=dc_stream_request_save_gas(view->stream,&chunk,&colors,channel,liquid,gas,generation);
        free(gas);
        fr_reactive_chunk_release(&reactive);
        if(!queued)
            return error(err, cap, "Cannot queue dirty chunk save");
    }
    return true;
}

static bool transfer_slot_matches(const dc_level_view_t *view, uint32_t index,
                                  uint64_t generation, dc_chunk_coord_t coord) {
    const dc_chunk_slot_t *slot = &view->table.slots[index];
    return slot->state != DC_SLOT_EMPTY && slot->generation == generation &&
           slot->coord.x == coord.x && slot->coord.y == coord.y;
}

static bool bind_transfer_destination(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view->transfer_pending || view->destination_bound) return true;
    uint32_t index;
    if (!dc_chunk_table_find(&view->table, view->transfer_to, &index)) {
        uint64_t generation;
        if (!dc_chunk_table_begin_load(&view->table, view->transfer_to,
                                       &index, &generation)) return true;
        if (!dc_stream_request_load(view->stream, view->transfer_to, generation))
            return error(err, cap, "Cannot load transfer destination");
    }
    view->to_slot = index;
    view->to_generation = view->table.slots[index].generation;
    view->table.slots[index].pinned = true;
    view->destination_bound = true;
    return true;
}

static bool resolve_world_transfer(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view->transfer_pending || !view->destination_bound) return true;
    if (!transfer_slot_matches(view, view->from_slot, view->from_generation,
                               view->transfer_from) ||
        !transfer_slot_matches(view, view->to_slot, view->to_generation,
                               view->transfer_to))
        return error(err, cap, "Pinned transfer chunk was reused");
    dc_chunk_slot_t *source = &view->table.slots[view->from_slot];
    dc_chunk_slot_t *destination = &view->table.slots[view->to_slot];
    if ((source->state != DC_SLOT_ACTIVE && source->state != DC_SLOT_SLEEPING) ||
        (destination->state != DC_SLOT_ACTIVE &&
         destination->state != DC_SLOT_SLEEPING)) return true;
    dc_gpu_transfer_t command = { .from_x = view->from_x, .from_y = view->from_y,
        .to_x = view->to_x, .to_y = view->to_y, .amount = view->transfer_amount,
        .kind = view->transfer_kind, .from_slot = view->from_slot,
        .to_slot = view->to_slot };
    dc_gpu_transfer_state_t state;
    if (!dc_gpu_queue_slot_transfer(view->gpu, command, err, cap) ||
        !dc_gpu_try_transfer(view->gpu, &state, err, cap)) return false;
    if (state == DC_GPU_TRANSFER_PENDING) return true;
    if (state == DC_GPU_TRANSFER_APPLIED &&
        (!dc_chunk_table_mark_dirty(&view->table, view->from_slot) ||
         !dc_chunk_table_mark_dirty(&view->table, view->to_slot)))
        return error(err, cap, "Cannot mark transferred chunks dirty");
    source->pinned = false;
    destination->pinned = false;
    view->transfer_pending = false;
    view->transfer_complete = true;
    view->transfer_state = state;
    return true;
}

bool dc_level_view_body_state(dc_level_view_t *view, uint32_t id,
                              dc_gpu_world_body_t *body, dc_gpu_body_motion_t *motion,
                              char *err, uint32_t cap) {
    return view && dc_gpu_read_world_body(view->gpu,id,body,err,cap) &&
        dc_gpu_read_body_motion(view->gpu,id,motion,err,cap);
}
bool dc_level_view_rigid_solver_stats(dc_level_view_t *view,dc_gpu_rigid_solver_stats_t *stats) {
    return view && dc_gpu_read_rigid_solver_stats(view->gpu,stats);
}

dc_level_view_t *dc_level_view_create(const char *directory, uint64_t seed,
                                      char *err, uint32_t cap) {
    return dc_level_view_create_ground(directory,seed,NULL,err,cap);
}

dc_level_view_t *dc_level_view_create_ground(const char *directory, uint64_t seed,
                                            const dc_ground_profile_t *ground,
                                            char *err, uint32_t cap) {
    Uint64 started=SDL_GetTicks64();
    dc_level_view_t *view = calloc(1, sizeof(*view));
    if (!view) { error(err, cap, "Out of memory creating level view"); return NULL; }
    view->origin.y = INITIAL_CHUNK_Y;
    view->mapped_origin.y = INITIAL_CHUNK_Y;
    if (!dc_chunk_table_init(&view->table, DC_GPU_CHUNK_SLOTS)) goto fail;
    bonfire_startup_trace("chunk table",started);
    if (!dc_gpu_create_window(&view->gpu, SIM_WIDTH, SIM_HEIGHT,
            VIEW_WIDTH * WINDOW_SCALE, VIEW_HEIGHT * WINDOW_SCALE,
            "build/shaders/pattern.comp.spv", err, cap)) goto fail;
    bonfire_startup_trace("GPU window",started);
    if (!dc_gpu_set_viewport(view->gpu, DC_CHUNK_SIDE, DC_CHUNK_SIDE,
                             VIEW_WIDTH, VIEW_HEIGHT) ||
        !dc_gpu_set_display_zoom(view->gpu, WINDOW_SCALE) ||
        !dc_gpu_set_fluid_interval(view->gpu, FLUID_INTERVAL) ||
        !dc_gpu_set_rigid_solver(view->gpu, true)) goto fail;
    bonfire_startup_trace("rigid solver",started);
    view->spring_enabled = true;
    view->stream = dc_stream_create_ground(directory, seed,
                                    DC_GPU_CHUNK_SLOTS * 2u, ground);
    if (!view->stream) { error(err, cap, "Cannot start chunk streaming worker"); goto fail; }
    bonfire_startup_trace("stream worker",started);
    int length = snprintf(view->body_path, sizeof(view->body_path), "%s/rigid_bodies.bin", directory);
    if (length < 0 || (size_t)length >= sizeof(view->body_path)) {
        error(err, cap, "World directory path is too long"); goto fail;
    }
    FILE *saved=fopen(view->body_path,"rb");
    view->restored=saved!=NULL;if(saved) fclose(saved);
    if (!dc_gpu_set_body_origin(view->gpu, (dc_chunk_coord_t){
            view->origin.x - HALO_CHUNKS, view->origin.y - HALO_CHUNKS }, err, cap) ||
        !fr_gpu_load_structure(view->gpu, view->body_path, err, cap) ||
        !dc_gpu_enable_automatic_components(view->gpu)) goto fail;
    bonfire_startup_trace("base level ready",started);
    return view;
fail:
    dc_stream_destroy(view->stream);
    dc_gpu_destroy(view->gpu);
    dc_chunk_table_destroy(&view->table);
    free(view);
    return NULL;
}

bool dc_level_view_tick(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view) return error(err, cap, "Level view is null");
    if (!process_results(view, err, cap)) return false;
    if (!bind_transfer_destination(view, err, cap)) return false;
    for (uint32_t i = 0; i < view->table.capacity; ++i) {
        dc_chunk_slot_t *slot = &view->table.slots[i];
        if (slot->state == DC_SLOT_ACTIVE || slot->state == DC_SLOT_SLEEPING)
            dc_chunk_table_set_active(&view->table, i, visible(view, slot->coord));
    }
    if (!schedule_saves(view, err, cap)) return false;
    for (uint32_t i = 0; i < view->table.capacity; ++i) {
        dc_chunk_slot_t *slot = &view->table.slots[i];
        if (slot->state == DC_SLOT_SLEEPING && !slot->dirty && !slot->pinned &&
            !dc_chunk_table_evict(&view->table, i, UINT64_MAX))
            return error(err, cap, "Cannot evict clean sleeping chunk");
    }
    if ((view->pending_chunk_dx || view->pending_chunk_dy) &&
        !dc_gpu_shift_velocity(view->gpu, view->pending_chunk_dx,
                                view->pending_chunk_dy, err, cap)) return false;
    bool all_resident = true;
    for (uint32_t y = 0; y < SIM_CHUNKS_Y; ++y) {
        for (uint32_t x = 0; x < SIM_CHUNKS_X; ++x) {
            dc_chunk_coord_t coord = {
                view->origin.x + (int64_t)x - HALO_CHUNKS,
                view->origin.y + (int64_t)y - HALO_CHUNKS };
            uint32_t index;
            if (!dc_chunk_table_find(&view->table, coord, &index)) {
                uint64_t generation;
                if (dc_chunk_table_begin_load(&view->table, coord, &index, &generation) &&
                    !dc_stream_request_load(view->stream, coord, generation))
                    return error(err, cap, "Cannot queue chunk load");
            }
            uint32_t page = UINT32_MAX;
            if (dc_chunk_table_find(&view->table, coord, &index) &&
                view->table.slots[index].state == DC_SLOT_ACTIVE) page = index;
            if (page == UINT32_MAX) all_resident = false;
            if (!dc_gpu_set_page(view->gpu, x, y, page, err, cap)) return false;
        }
    }
    view->pending_chunk_dx = 0;
    view->pending_chunk_dy = 0;
    view->mapped_origin = view->origin;
    if(view->smoke_ready&&!fr_gas_gpu_initialize_queued(view->gpu,err,cap)) return false;
    dc_chunk_coord_t body_origin={view->origin.x-HALO_CHUNKS,view->origin.y-HALO_CHUNKS};
    bool ticking=all_resident&&view->pending_steps!=0u&&!view->transfer_pending;
    if(!(ticking?dc_gpu_prepare_body_origin(view->gpu,body_origin,err,cap):
                 dc_gpu_set_body_origin(view->gpu,body_origin,err,cap))) return false;
    if (!resolve_world_transfer(view, err, cap)) return false;
    if (view->spring_enabled && view->origin.x - HALO_CHUNKS <= 2 * WORLD_SCALE &&
        view->origin.x + VIEW_CHUNKS_X + HALO_CHUNKS > 2 * WORLD_SCALE &&
        view->origin.y - HALO_CHUNKS <= 0 &&
        view->origin.y + VIEW_CHUNKS_Y + HALO_CHUNKS > 0) {
        uint32_t x = (uint32_t)(2 * WORLD_SCALE - view->origin.x + HALO_CHUNKS) * DC_CHUNK_SIDE;
        uint32_t y = (uint32_t)(-view->origin.y + HALO_CHUNKS) * DC_CHUNK_SIDE +
                     4u * WORLD_SCALE;
        dc_gpu_set_tick_water_source(view->gpu, true, x, y);
    } else dc_gpu_set_tick_water_source(view->gpu, false, 0, 0);
    uint32_t ready_steps = all_resident ? view->pending_steps : 0u;
    if (ready_steps && !dc_gpu_set_tick_seconds(view->gpu,
            view->pending_seconds / (float)ready_steps))
        return error(err, cap, "Invalid elapsed simulation time");
    if (!dc_gpu_present_chunks_steps(view->gpu, ready_steps, err, cap)) return false;
    view->pending_steps = 0;
    view->pending_seconds = 0.0f;
    return true;
}

bool dc_level_view_wait_visible(dc_level_view_t *view, uint32_t timeout_ms,
                                char *err, uint32_t cap) {
    uint64_t deadline = SDL_GetTicks64() + timeout_ms;
    do {
        if (!dc_level_view_tick(view, err, cap)) return false;
        bool ready = true;
        for (uint32_t y = 0; y < SIM_CHUNKS_Y; ++y) {
            for (uint32_t x = 0; x < SIM_CHUNKS_X; ++x) {
                dc_chunk_coord_t coord = {
                    view->origin.x + (int64_t)x - HALO_CHUNKS,
                    view->origin.y + (int64_t)y - HALO_CHUNKS };
                uint32_t index;
                if (!dc_chunk_table_find(&view->table, coord, &index) ||
                    view->table.slots[index].state != DC_SLOT_ACTIVE) ready = false;
            }
        }
        if (ready) return true;
        SDL_Delay(1);
    } while (SDL_GetTicks64() < deadline);
    return error(err, cap, "Timed out waiting for visible chunks");
}

bool dc_level_view_move(dc_level_view_t *view, int32_t dx, int32_t dy) {
    if (!view || dx < -1 || dx > 1 || dy < -1 || dy > 1) return false;
    if ((dx < 0 && view->origin.x <= INT64_MIN + HALO_CHUNKS) ||
        (dy < 0 && view->origin.y <= INT64_MIN + HALO_CHUNKS) ||
        (dx > 0 && view->origin.x >= INT64_MAX - VIEW_CHUNKS_X - HALO_CHUNKS) ||
        (dy > 0 && view->origin.y >= INT64_MAX - VIEW_CHUNKS_Y - HALO_CHUNKS)) return false;
    view->origin.x += dx;
    view->origin.y += dy;
    view->pending_chunk_dx += dx;
    view->pending_chunk_dy += dy;
    return true;
}

bool dc_level_view_pan_pixels(dc_level_view_t *view, int32_t dx, int32_t dy) {
    if (!view || dx < -1024 || dx > 1024 || dy < -1024 || dy > 1024)
        return false;
    dc_chunk_coord_t shift;
    uint32_t offset_x, offset_y;
    dc_cell_address((int64_t)view->camera_offset_x + dx,
                    (int64_t)view->camera_offset_y + dy,
                    &shift, &offset_x, &offset_y);
    const int64_t min_origin = INT64_MIN + HALO_CHUNKS;
    const int64_t max_x = INT64_MAX - VIEW_CHUNKS_X - HALO_CHUNKS;
    const int64_t max_y = INT64_MAX - VIEW_CHUNKS_Y - HALO_CHUNKS;
    if ((shift.x < 0 && view->origin.x < min_origin - shift.x) ||
        (shift.x > 0 && view->origin.x > max_x - shift.x) ||
        (shift.y < 0 && view->origin.y < min_origin - shift.y) ||
        (shift.y > 0 && view->origin.y > max_y - shift.y) ||
        (shift.x > 0 && view->pending_chunk_dx > INT32_MAX - shift.x) ||
        (shift.x < 0 && view->pending_chunk_dx < INT32_MIN - shift.x) ||
        (shift.y > 0 && view->pending_chunk_dy > INT32_MAX - shift.y) ||
        (shift.y < 0 && view->pending_chunk_dy < INT32_MIN - shift.y))
        return false;
    if (!dc_gpu_set_viewport(view->gpu, DC_CHUNK_SIDE + offset_x,
                             DC_CHUNK_SIDE + offset_y,
                             VIEW_WIDTH, VIEW_HEIGHT)) return false;
    view->origin.x += shift.x;
    view->origin.y += shift.y;
    view->camera_offset_x = offset_x;
    view->camera_offset_y = offset_y;
    view->pending_chunk_dx += (int32_t)shift.x;
    view->pending_chunk_dy += (int32_t)shift.y;
    return true;
}

bool dc_level_view_reset_camera(dc_level_view_t *view) {
    if (!view || !dc_gpu_set_viewport(view->gpu, DC_CHUNK_SIDE,
                                       DC_CHUNK_SIDE, VIEW_WIDTH,
                                       VIEW_HEIGHT)) return false;
    view->pending_chunk_dx = view->mapped_origin.x > SIM_CHUNKS_X ?
        -SIM_CHUNKS_X : view->mapped_origin.x < -SIM_CHUNKS_X ?
        SIM_CHUNKS_X : (int32_t)-view->mapped_origin.x;
    int64_t mapped_offset_y = view->mapped_origin.y - INITIAL_CHUNK_Y;
    view->pending_chunk_dy = mapped_offset_y > SIM_CHUNKS_Y ?
        -SIM_CHUNKS_Y : mapped_offset_y < -SIM_CHUNKS_Y ?
        SIM_CHUNKS_Y : (int32_t)-mapped_offset_y;
    view->origin = (dc_chunk_coord_t){0, INITIAL_CHUNK_Y};
    view->camera_offset_x = 0;
    view->camera_offset_y = 0;
    return true;
}

bool dc_level_view_status(dc_level_view_t *view, dc_level_view_status_t *status) {
    if (!view || !status) return false;
    *status = (dc_level_view_status_t){ .origin = view->origin,
        .offset_x = view->camera_offset_x,
        .offset_y = view->camera_offset_y,
        .total_chunks = SIM_CHUNKS_X * SIM_CHUNKS_Y };
    for (uint32_t y = 0; y < SIM_CHUNKS_Y; ++y)
        for (uint32_t x = 0; x < SIM_CHUNKS_X; ++x) {
            dc_chunk_coord_t coord = {
                view->origin.x + (int64_t)x - HALO_CHUNKS,
                view->origin.y + (int64_t)y - HALO_CHUNKS };
            uint32_t index;
            if (dc_chunk_table_find(&view->table, coord, &index) &&
                view->table.slots[index].state == DC_SLOT_ACTIVE)
                ++status->ready_chunks;
        }
    return true;
}

bool dc_level_view_set_title(dc_level_view_t *view, const char *title) {
    return view && dc_gpu_set_window_title(view->gpu, title);
}

bool dc_level_view_toggle_marker_overlay(dc_level_view_t *view) {
    if (!view) return false;
    view->marker_overlay = !view->marker_overlay;
    return dc_gpu_set_marker_overlay(view->gpu, view->marker_overlay);
}

bool dc_level_view_set_zoom(dc_level_view_t *view, uint32_t zoom) {
    return view && dc_gpu_set_display_zoom(view->gpu, zoom);
}
bool dc_level_view_bonfire_presentation(dc_level_view_t *view,uint32_t focus,bool paused) {
    if(!view) return false;
    dc_presentation_focus_t crop=focus<2?(dc_presentation_focus_t){focus?312u:120u,144,80,96}:(dc_presentation_focus_t){0};
    return dc_gpu_set_presentation_focus(view->gpu,crop)&&fr_combustion_gpu_hud(view->gpu,true,focus==1,paused,
        focus<2?crop.x:4u,focus<2?crop.y:4u);
}

bool dc_level_view_screen_cell(dc_level_view_t *view, uint32_t screen_x,
                               uint32_t screen_y, uint32_t *cell_x,
                               uint32_t *cell_y) {
    return view && dc_gpu_screen_cell(view->gpu, screen_x, screen_y,
                                      cell_x, cell_y);
}

bool dc_level_view_set_overlay(dc_level_view_t *view, dc_gpu_overlay_t overlay) {
    return view && dc_gpu_set_overlay(view->gpu, overlay);
}

bool dc_level_view_set_spring_enabled(dc_level_view_t *view, bool enabled) {
    if (!view) return false;
    view->spring_enabled = enabled;
    return true;
}

bool dc_level_view_set_marker_correction(dc_level_view_t *view, bool enabled) {
    return view && dc_gpu_set_marker_correction(view->gpu, enabled);
}

bool dc_level_view_set_fluid_interval(dc_level_view_t *view, uint32_t interval) {
    return view && dc_gpu_set_fluid_interval(view->gpu, interval);
}

static int32_t cell_chunk_offset(int32_t coordinate) {
    return coordinate >= 0 ? coordinate / DC_CHUNK_SIDE :
           -((-coordinate + DC_CHUNK_SIDE - 1) / DC_CHUNK_SIDE);
}

bool dc_level_view_paint(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t radius, uint16_t material, char *err, uint32_t cap) {
    if (!view || x >= VIEW_WIDTH || y >= VIEW_HEIGHT || radius > 16)
        return error(err, cap, "Invalid material brush coordinates");
    if (!dc_gpu_paint_material(view->gpu, x + DC_CHUNK_SIDE + view->camera_offset_x,
                               y + DC_CHUNK_SIDE + view->camera_offset_y,
                               radius, material, err, cap)) return false;
    int32_t first_x = cell_chunk_offset((int32_t)x +
        (int32_t)view->camera_offset_x - (int32_t)radius);
    int32_t first_y = cell_chunk_offset((int32_t)y +
        (int32_t)view->camera_offset_y - (int32_t)radius);
    int32_t last_x = cell_chunk_offset((int32_t)x +
        (int32_t)view->camera_offset_x + (int32_t)radius);
    int32_t last_y = cell_chunk_offset((int32_t)y +
        (int32_t)view->camera_offset_y + (int32_t)radius);
    for (int32_t ty = first_y; ty <= last_y; ++ty) {
        for (int32_t tx = first_x; tx <= last_x; ++tx) {
            dc_chunk_coord_t coord = { view->origin.x + tx, view->origin.y + ty };
            uint32_t index;
            if (dc_chunk_table_find(&view->table, coord, &index) &&
                view->table.slots[index].state == DC_SLOT_ACTIVE)
                dc_chunk_table_mark_dirty(&view->table, index);
        }
    }
    return true;
}

bool dc_level_view_pixel(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t *color, char *err, uint32_t cap) {
    if (!view || !color || x >= VIEW_WIDTH || y >= VIEW_HEIGHT)
        return error(err, cap, "Invalid pixel readback");
    uint32_t *pixels = malloc((size_t)VIEW_WIDTH * VIEW_HEIGHT * sizeof(*pixels));
    if (!pixels) return error(err, cap, "Out of memory reading level pixel");
    bool okay = dc_gpu_readback(view->gpu, pixels,
                                VIEW_WIDTH * VIEW_HEIGHT, err, cap);
    if (okay) *color = pixels[y * VIEW_WIDTH + x];
    free(pixels);
    return okay;
}

bool dc_level_view_pixels(dc_level_view_t *view, uint32_t *colors,
                          uint32_t count, char *err, uint32_t cap) {
    if (!view || !colors || count < VIEW_WIDTH * VIEW_HEIGHT)
        return error(err, cap, "Invalid level image readback");
    return dc_gpu_readback(view->gpu, colors, count, err, cap);
}

bool dc_level_view_capture_tick(dc_level_view_t *view,
                                dc_gpu_tick_capture_t *capture,
                                char *err, uint32_t cap) {
    return view && dc_gpu_tick_capture(view->gpu, capture, err, cap);
}

bool dc_level_view_memory_stats(dc_level_view_t *view,
                                dc_gpu_memory_stats_t *stats) {
    return view && dc_gpu_memory_stats(view->gpu, stats);
}

bool dc_level_view_chunk(dc_level_view_t *view, dc_chunk_coord_t coord,
                         dc_chunk_t *chunk, char *err, uint32_t cap) {
    uint32_t index;
    if (!view || !chunk || !dc_chunk_table_find(&view->table, coord, &index) ||
        (view->table.slots[index].state != DC_SLOT_ACTIVE &&
         view->table.slots[index].state != DC_SLOT_SLEEPING))
        return error(err, cap, "Requested chunk is not resident");
    chunk->coord = coord;
    return dc_gpu_download_chunk(view->gpu, index, chunk, err, cap);
}

bool dc_level_view_has_chunk(dc_level_view_t *view, dc_chunk_coord_t coord) {
    return view && dc_chunk_table_find(&view->table, coord, NULL);
}

bool dc_level_view_queue_transfer(dc_level_view_t *view, int64_t from_x, int64_t from_y,
                                  int64_t to_x, int64_t to_y, uint32_t amount,
                                  dc_gpu_transfer_kind_t kind, char *err, uint32_t cap) {
    if (!view || view->transfer_pending ||
        (kind != DC_GPU_TRANSFER_SCALAR && kind != DC_GPU_TRANSFER_PARTICLE) ||
        (kind == DC_GPU_TRANSFER_SCALAR && (!amount || amount > DC_FLUID_FULL)))
        return error(err, cap, "Invalid world transfer");
    bool adjacent =
        (from_y == to_y &&
         ((from_x < INT64_MAX && to_x == from_x + 1) ||
          (from_x > INT64_MIN && to_x == from_x - 1))) ||
        (from_x == to_x &&
         ((from_y < INT64_MAX && to_y == from_y + 1) ||
          (from_y > INT64_MIN && to_y == from_y - 1)));
    if (!adjacent) return error(err, cap, "World transfer cells must be adjacent");
    dc_chunk_coord_t from, to;
    uint32_t from_local_x, from_local_y, to_local_x, to_local_y;
    dc_cell_address(from_x, from_y, &from, &from_local_x, &from_local_y);
    dc_cell_address(to_x, to_y, &to, &to_local_x, &to_local_y);
    uint32_t index;
    if (!dc_chunk_table_find(&view->table, from, &index) ||
        (view->table.slots[index].state != DC_SLOT_ACTIVE &&
         view->table.slots[index].state != DC_SLOT_SLEEPING) ||
        view->table.slots[index].pinned)
        return error(err, cap, "Transfer source is not available");
    view->from_slot = index;
    view->from_generation = view->table.slots[index].generation;
    view->table.slots[index].pinned = true;
    view->transfer_from = from;
    view->transfer_to = to;
    view->from_x = from_local_x;
    view->from_y = from_local_y;
    view->to_x = to_local_x;
    view->to_y = to_local_y;
    view->transfer_amount = amount;
    view->transfer_kind = kind;
    view->destination_bound = false;
    view->transfer_complete = false;
    view->transfer_pending = true;
    return bind_transfer_destination(view, err, cap);
}

bool dc_level_view_transfer_result(dc_level_view_t *view,
                                   dc_gpu_transfer_state_t *state) {
    if (!view || !state || !view->transfer_complete) return false;
    *state = view->transfer_state;
    return true;
}

bool dc_level_view_destroy(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view) return true;
    bool okay = true;
    if (!fr_gpu_save_structure(view->gpu, view->body_path, err, cap)) okay = false;
    if (view->transfer_pending) {
        view->table.slots[view->from_slot].pinned = false;
        if (view->destination_bound)
            view->table.slots[view->to_slot].pinned = false;
        view->transfer_pending = false;
    }
    for (uint32_t i = 0; i < view->table.capacity; ++i)
        if (view->table.slots[i].state == DC_SLOT_ACTIVE)
            dc_chunk_table_set_active(&view->table, i, false);
    uint64_t deadline = SDL_GetTicks64() +
                        (uint64_t)view->table.capacity * 50u + 5000u;
    for (;;) {
        if (!process_results(view, err, cap) || !schedule_saves(view, err, cap)) { okay = false; break; }
        bool pending = false;
        for (uint32_t i = 0; i < view->table.capacity; ++i) {
            dc_chunk_slot_t *slot = &view->table.slots[i];
            if (slot->dirty || slot->state == DC_SLOT_SAVING) pending = true;
        }
        if (!pending) break;
        if (SDL_GetTicks64() >= deadline) {
            okay = error(err, cap, "Timed out saving modified chunks"); break;
        }
        SDL_Delay(1);
    }
    dc_stream_destroy(view->stream);
    dc_gpu_destroy(view->gpu);
    dc_chunk_table_destroy(&view->table);
    free(view);
    return okay;
}
