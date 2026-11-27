#ifndef DC_REACTIVE_STREAM_CAMERA_CASES_H
#define DC_REACTIVE_STREAM_CAMERA_CASES_H
#include "../../src/app/offscreen_internal.h"

static void test_camera_eviction_keeps_the_published_owner_across_archive_rewrites(void) {
    char directory[] = "build/camera_durable_owner_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t view = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk && structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(view.gpu));
    ASSERT_TRUE(dc_chunk_table_init(&view.table, DC_GPU_CHUNK_SLOTS));
    view.stream = dc_stream_create(directory, 42, 4);
    ASSERT_TRUE(view.stream);
    snprintf(view.body_path, sizeof(view.body_path), "%s/%s", directory, DC_WORLD_BODY_RECORD);

    const dc_chunk_coord_t coord = {-9, 7};
    chunk->coord = coord;
    chunk->cells[23].fluid_mass = 14000;
    uint32_t slot;
    uint64_t generation;
    ASSERT_TRUE(dc_chunk_table_begin_load(&view.table, coord, &slot, &generation));
    ASSERT_TRUE(dc_gpu_upload_chunk(view.gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_finish_load(&view.table, slot, generation));
    view.table.slots[slot].time = (dc_chunk_time_t){2., true};
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&view.table, slot));
    ASSERT_TRUE(dc_chunk_table_set_active(&view.table, slot, false));
    ASSERT_TRUE(schedule_saves(&view, err, sizeof(err)));
    ASSERT_EQ(view.table.slots[slot].state, DC_SLOT_SAVING);
    ASSERT_TRUE(save_level_structure(&view, err, sizeof(err)));

    dc_stream_result_t reply = {0};
    ASSERT_TRUE(receive(view.stream, &reply));
    ASSERT_EQ(reply.kind, DC_STREAM_SAVED);
    dc_chunk_owner_t written = reply.replaces_owner;
    dc_stream_result_release(&reply);
    ASSERT_TRUE(dc_stream_request_load(view.stream, coord, generation));
    ASSERT_TRUE(receive(view.stream, &reply));
    ASSERT_EQ(reply.kind, DC_STREAM_LOADED);
    ASSERT_EQ(reply.chunk->cells[23].fluid_mass, 14000u);
    ASSERT_TRUE(reply.time.known && reply.time.seconds == 2.);
    ASSERT_TRUE(dc_chunk_owner_equal(written, reply.replaces_owner));
    dc_stream_result_release(&reply);
    dc_stream_destroy(view.stream);
    dc_gpu_destroy(view.gpu);
    dc_chunk_table_destroy(&view.table);
    free(chunk);

    view.gpu = NULL;
    ASSERT_TRUE(structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(view.gpu, view.body_path, err, sizeof(err)));
    ASSERT_TRUE(view.gpu->checkpoint_chunk_count == 1 && view.gpu->checkpoint_owners);
    const dc_chunk_owner_t archive = view.gpu->checkpoint_owners[0];
    ASSERT_EQ(dc_gpu_validation_errors(view.gpu), 0u);
    dc_gpu_destroy(view.gpu);
    ASSERT_TRUE(dc_chunk_owner_valid(written));
    ASSERT_TRUE(dc_chunk_owner_equal(written, archive));
    PASS();
}

static dc_level_view_t *camera_test_view(const char *directory, bool restored, char *err, uint32_t cap) {
    dc_level_view_t *view = calloc(1, sizeof(*view));
    if (!view) return NULL;
    view->origin = view->mapped_origin = (dc_chunk_coord_t){0, -20};
    view->world_seconds = 2.;
    view->save = dc_world_save_open(directory, err, cap);
    view->stream = dc_stream_create(directory, 42, DC_GPU_CHUNK_SLOTS * 2u);
    snprintf(view->body_path, sizeof(view->body_path), "%s/%s", directory, DC_WORLD_BODY_RECORD);
    bool okay = view->save && view->stream && context(&view->gpu, err, cap) &&
        dc_gpu_set_viewport(view->gpu, DC_CHUNK_SIDE, DC_CHUNK_SIDE, VIEW_WIDTH, VIEW_HEIGHT) &&
        dc_chunk_table_init(&view->table, DC_GPU_CHUNK_SLOTS) && dc_gpu_enable_automatic_components(view->gpu);
    if (okay && restored) okay = dc_world_save_load_clock(view->save, &view->world_seconds, err, cap) &&
        fr_gpu_load_structure(view->gpu, view->body_path, err, cap);
    if (okay) return view;
    dc_level_view_discard(view);
    return NULL;
}

static uint64_t camera_stock(const dc_chunk_t *chunk, const fr_bulk_phase_chunk_t *bulk, bool oil) {
    uint64_t total = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        total += oil ? bulk->oil_fp[i] : chunk->cells[i].fluid_mass;
    return total;
}

static void camera_seal_room(dc_chunk_t *chunk, dc_chunk_coord_t coord) {
    chunk->coord = coord;
    for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y) {
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
            if (x && y && x + 1 < DC_CHUNK_SIDE && y + 1 < DC_CHUNK_SIDE) continue;
            chunk->cells[y * DC_CHUNK_SIDE + x] = (dc_cell_t){
                .material = DC_MATERIAL_STONE, .flags = DC_CELL_LOADED_TERRAIN};
        }
    }
}

static int camera_marker_order(const void *left, const void *right) {
    return memcmp(left, right, sizeof(dc_marker_t));
}

enum { CAMERA_HEAT=1u, CAMERA_BULK=2u, CAMERA_REOPEN=4u, CAMERA_GRAINS=8u, CAMERA_GAS=16u };

static bool camera_gas_enable(dc_gpu_t *gpu,char *err,uint32_t cap) {
    const fr_gas_model_t model={.gas_constant={1,1,1,1,0},.heat_capacity_cv={2.5,2.5,2.5,2.5,1},.kinetic_scale=1};
    return fr_gas_gpu_enable(gpu,&model,1.f,300.f,err,cap);
}
static bool camera_seed_gas(dc_gpu_t *gpu,uint32_t slot,const dc_chunk_t *chunk,bool donor,char *err,uint32_t cap) {
    fr_gas_state_t *states=calloc(DC_CHUNK_CELLS,sizeof(*states));
    double *volume=calloc(DC_CHUNK_CELLS,sizeof(*volume));
    bool okay=states && volume;
    for(unsigned i=0;okay && i<DC_CHUNK_CELLS;i++) {
        if (chunk->cells[i].material==DC_MATERIAL_STONE) continue;
        double v=1.-chunk->cells[i].fluid_mass/(double)DC_FLUID_FULL;
        volume[i]=v;
        states[i]=(fr_gas_state_t){.species={.2*v,.8*v,donor?.125*v:0.,0.,0.},
            .energy=(donor?1.125:1.)*v*2.5*300.,.pigment={donor?.05*v:0.,0.,0.}};
    }
    if (okay) okay=fr_gas_gpu_seed_chunk(gpu,slot,states,volume,DC_CHUNK_CELLS,err,cap);
    free(volume); free(states);
    return okay;
}

static void check_camera_water_returns(unsigned channels) {
    bool heated=channels&CAMERA_HEAT,bulk_liquid=channels&CAMERA_BULK,restart=channels&CAMERA_REOPEN;
    bool grains=channels&CAMERA_GRAINS,gas=channels&CAMERA_GAS;
    char directory[] = "build/camera_water_basin_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(view && chunk && dc_level_view_set_fluid_interval(view, 2u));
    const dc_chunk_coord_t first = {view->origin.x - HALO_CHUNKS, view->origin.y};
    const uint32_t row = 32u * DC_CHUNK_SIDE;
    dc_gpu_t *reference = NULL;
    ASSERT_TRUE(dc_gpu_create(&reference, 2u * DC_CHUNK_SIDE, DC_CHUNK_SIDE,
        "build/shaders/pattern.comp.spv", err, sizeof(err)) &&
        dc_gpu_set_body_origin(reference, first, err, sizeof(err)) &&
        dc_gpu_set_fluid_interval(reference, 2u));
    dc_chunk_colors_t colors = {0}, expected_colors;
    float heat[DC_CHUNK_CELLS] = {0}, expected_heat[DC_CHUNK_CELLS];
    fr_bulk_phase_chunk_t *phase = calloc(1, sizeof(*phase)), *expected_phase = calloc(1, sizeof(*expected_phase));
    ASSERT_TRUE(phase && expected_phase);
    if (gas) {
        ASSERT_TRUE(camera_gas_enable(view->gpu,err,sizeof(err)) && camera_gas_enable(reference,err,sizeof(err)));
        view->stream_ambient_gas=true;
    }
    if (heated) ASSERT_TRUE(fr_thermal_gpu_enable(view->gpu, err, sizeof(err)) &&
                            fr_thermal_gpu_enable(reference, err, sizeof(err)));
    for (unsigned tile = 0; tile < 2u; ++tile) {
        memset(chunk, 0, sizeof(*chunk));
        chunk->coord = (dc_chunk_coord_t){first.x + tile, first.y};
        for (unsigned i = 0; i < DC_CHUNK_CELLS; ++i)
            chunk->cells[i] = (dc_cell_t){.material = DC_MATERIAL_STONE, .flags = DC_CELL_LOADED_TERRAIN};
        for (unsigned x = tile ? 0u : 48u; x < (tile ? 48u : DC_CHUNK_SIDE); ++x) {
            chunk->cells[row+x] = (dc_cell_t){.fluid_mass = !tile && x >= 53u ? DC_FLUID_FULL/2u : 0u};
            chunk->face_velocity[row+x].x = 7.5f;
        }
        if (grains) {
            for (unsigned y=8;y<18;y++)
                for (unsigned x=tile?0u:48u;x<(tile?48u:64u);x++)
                    chunk->cells[y*64u+x]=(dc_cell_t){0};
            if (!tile) {
                unsigned grain=10u*64u+59u;
                chunk->cells[grain].material=DC_MATERIAL_SAND;
                dc_chunk_seed_particles(chunk);
                chunk->particles[grain].vx_fp=2*DC_FLUID_FULL;
            }
        }
        if (!tile) {
            chunk->marker_count = 1;
            chunk->markers[0] = (dc_marker_t){62u * DC_FLUID_FULL, 32u * DC_FLUID_FULL + DC_FLUID_FULL / 2u,
                                               1001u, DC_MARKER_INSIDE};
        }
        memset(&colors, 0, sizeof(colors));
        memset(heat, 0, sizeof(heat));
        memset(phase, 0, sizeof(*phase));
        for (unsigned i = 0; i < DC_CHUNK_CELLS; ++i) {
            if (!chunk->cells[i].fluid_mass) continue;
            colors.water[i][1] = .75f;
            colors.water[i][3] = 1.f;
            heat[i] = 2.f + .25f * (i % DC_CHUNK_SIDE);
            if (bulk_liquid) {
                phase->oil_fp[i] = DC_FLUID_FULL / 4u;
                phase->slime_fp[i] = DC_FLUID_FULL / 8u;
                phase->oil_momentum_x[i] = 4.f;
                phase->slime_momentum_x[i] = 8.f;
            }
        }
        ASSERT_TRUE(dc_gpu_upload_chunk(reference, tile, chunk, err, sizeof(err)) &&
            dc_gpu_set_page(reference, tile, 0, tile, err, sizeof(err)) &&
            fr_renderer_upload_chunk_colors(reference, tile, &colors, err, sizeof(err)));
        uint32_t slot;
        uint64_t generation;
        ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, chunk->coord, &slot, &generation) &&
            dc_gpu_upload_chunk(view->gpu, slot, chunk, err, sizeof(err)) &&
            fr_renderer_upload_chunk_colors(view->gpu, slot, &colors, err, sizeof(err)) &&
            dc_chunk_table_finish_load(&view->table, slot, generation));
        if (gas) {
            ASSERT_TRUE(camera_seed_gas(view->gpu,slot,chunk,!tile,err,sizeof(err)) &&
                camera_seed_gas(reference,tile,chunk,!tile,err,sizeof(err)));
            view->gas_admitted[slot]=true;
        }
        if (grains && !tile) {
            const unsigned index=10u*64u+59u;
            fr_reactive_state_t state;
            ASSERT_TRUE(fr_reactive_state_from_particle(chunk->particles+index,475.f,901u,&state,err,sizeof(err)) &&
                fr_gpu_reactive_seed_particle(view->gpu,slot*DC_MPM_PARTICLES_PER_CHUNK+index,&state,err,sizeof(err)) &&
                fr_gpu_reactive_seed_particle(reference,index,&state,err,sizeof(err)));
        }
        view->table.slots[slot].time = (dc_chunk_time_t){view->world_seconds, true};
        if (bulk_liquid) ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(view->gpu, slot, chunk, phase, err, sizeof(err)) &&
            fr_bulk_phase_gpu_import_chunk(reference, tile, chunk, phase, err, sizeof(err)));
        if (heated) ASSERT_TRUE(fr_thermal_gpu_import_water_chunk(view->gpu, slot, chunk, heat, err, sizeof(err)) &&
                                fr_thermal_gpu_import_water_chunk(reference, tile, chunk, heat, err, sizeof(err)));
        ASSERT_TRUE(dc_chunk_table_mark_dirty(&view->table, slot));
    }
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    const float elapsed = 1.f / 60.f;
    unsigned ticks = 0u;
    for (unsigned cycle = 0; cycle < 3u; ++cycle) {
        if (cycle != 1u) {
            ASSERT_TRUE(dc_level_view_step_timed(view, elapsed, err, sizeof(err)) &&
                        dc_level_view_tick(view, err, sizeof(err)));
            ASSERT_EQ(view->gpu->fluid_phase, 3u);
            ASSERT_TRUE(dc_level_view_step_timed(view, elapsed, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_set_tick_seconds(reference, elapsed) && dc_gpu_tick_step(reference, err, sizeof(err)) &&
                        dc_gpu_tick_step(reference, err, sizeof(err)));
            ticks += 2u;
        }
        ASSERT_TRUE(dc_level_view_move(view, 1, 0));
        if (cycle == 1u) {
            uint32_t departing;
            ASSERT_TRUE(dc_chunk_table_find(&view->table, first, &departing));
            ASSERT_TRUE(dc_chunk_table_set_active(&view->table, departing, false) &&
                        schedule_saves(view, err, sizeof(err)));
            const uint64_t deadline = SDL_GetTicks64() + 5000u;
            while (view->camera_offscreen.phase == DC_CAMERA_OFFSCREEN_SAVING && SDL_GetTicks64() < deadline) {
                ASSERT_TRUE(process_results(view, err, sizeof(err)));
                SDL_Delay(1);
            }
            ASSERT_EQ(view->camera_offscreen.phase, DC_CAMERA_OFFSCREEN_STORED);
            ASSERT_TRUE(dc_chunk_table_evict(&view->table, departing, UINT64_MAX));
            const uint64_t next = view->table.next_generation;
            view->table.next_generation = UINT64_MAX;
            ASSERT_TRUE(!dc_camera_offscreen_service(view, false, err, sizeof(err)));
            ASSERT_EQ(view->camera_offscreen.phase, DC_CAMERA_OFFSCREEN_STORED);
            ASSERT_TRUE(!view->camera_offscreen.workspace);
            ASSERT_EQ(view->world_seconds, 2. + ticks * (double)elapsed);
            view->table.next_generation = next;
            ASSERT_TRUE(dc_camera_offscreen_service(view, false, err, sizeof(err)));
            dc_stream_result_t failure = {.kind = DC_STREAM_FAILED, .coord = first,
                .generation = view->camera_offscreen.generation};
            ASSERT_EQ(dc_level_offscreen_result(view, &failure, err, sizeof(err)), DC_LEVEL_OFFSCREEN_FAILED);
            ASSERT_EQ(view->camera_offscreen.phase, DC_CAMERA_OFFSCREEN_STORED);
            ASSERT_TRUE(!dc_level_offscreen_owns(view, first));
        }
        ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
        ASSERT_TRUE(dc_level_offscreen_owns(view, first));
        if (gas && cycle == 0u) {
            /* Isolate the far halo rooms before they acquire older disk epochs. */
            for (uint32_t i = 0; i < view->table.capacity; ++i) {
                const dc_chunk_slot_t *s = &view->table.slots[i];
                if (s->state != DC_SLOT_ACTIVE || s->coord.x !=
                    view->origin.x + VIEW_CHUNKS_X + HALO_CHUNKS - 1) continue;
                memset(chunk, 0, sizeof(*chunk));
                camera_seal_room(chunk, s->coord);
                ASSERT_TRUE(dc_gpu_upload_chunk(view->gpu, i, chunk, err, sizeof(err)) &&
                    camera_seed_gas(view->gpu, i, chunk, false, err, sizeof(err)) &&
                    dc_chunk_table_mark_dirty(&view->table, i));
            }
        }
        ASSERT_TRUE(dc_level_view_step_timed(view, elapsed, err, sizeof(err)));
        uint32_t older_slot = UINT32_MAX;
        double older_start = 0.;
        if (cycle == 2u) {
            for (uint32_t i = 0; i < view->table.capacity; ++i) {
                const dc_chunk_slot_t *s = &view->table.slots[i];
                if (s->state == DC_SLOT_ACTIVE && s->time.known && s->time.seconds < view->world_seconds) {
                    older_slot = i;
                    older_start = s->time.seconds;
                    break;
                }
            }
            ASSERT_TRUE(older_slot != UINT32_MAX);
            memset(chunk, 0, sizeof(*chunk));
            camera_seal_room(chunk, view->table.slots[older_slot].coord);
            chunk->cells[row+32u].fluid_mass = DC_FLUID_FULL;
            chunk->face_velocity[row+32u].x = 5.f;
            ASSERT_TRUE(dc_gpu_upload_chunk(view->gpu, older_slot, chunk, err, sizeof(err)));
            uint32_t connected;
            ASSERT_TRUE(dc_chunk_table_find(&view->table, (dc_chunk_coord_t){first.x+1,first.y}, &connected));
            double start = view->world_seconds;
            view->table.slots[connected].time.seconds = older_start;
            ASSERT_TRUE(!dc_level_view_tick(view, err, sizeof(err)));
            ASSERT_EQ(view->world_seconds, start);
            ASSERT_EQ(view->table.slots[older_slot].time.seconds, older_start);
            ASSERT_EQ(view->pending_steps, 1u);
            view->table.slots[connected].time.seconds = start;
        }
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
        if (older_slot != UINT32_MAX) {
            ASSERT_EQ(view->table.slots[older_slot].time.seconds, older_start + elapsed);
            ASSERT_TRUE(view->table.slots[older_slot].time.seconds < view->world_seconds);
        }
        ASSERT_TRUE(dc_gpu_liquid_owner_held(view->gpu));
        if (restart && cycle == 0u) {
            const dc_chunk_coord_t saved_origin = view->origin;
            const double saved_time = view->world_seconds;
            ASSERT_EQ(view->pending_steps, 0u);
            ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
            view = dc_level_view_create(directory, 42, err, sizeof(err));
            ASSERT_TRUE(view && view->origin.x == saved_origin.x && view->origin.y == saved_origin.y);
            ASSERT_EQ(view->world_seconds, saved_time);
            if (gas) {
                ASSERT_TRUE(camera_gas_enable(view->gpu,err,sizeof(err)));
                view->stream_ambient_gas=true;
            }
            if (heated) ASSERT_TRUE(fr_thermal_gpu_enable(view->gpu, err, sizeof(err)));
            ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_liquid_owner_held(view->gpu));
        }
        if (cycle == 2u) {
            const double start = view->world_seconds;
            uint32_t slot;
            ASSERT_TRUE(dc_chunk_table_find(&view->table, (dc_chunk_coord_t){first.x+1,first.y}, &slot));
            const uint64_t pinned = view->water_generations[slot];
            ASSERT_TRUE(pinned);
            view->water_generations[slot] = pinned - 1u;
            ASSERT_TRUE(dc_level_view_step_timed(view, elapsed, err, sizeof(err)));
            ASSERT_TRUE(!dc_level_view_tick(view, err, sizeof(err)));
            ASSERT_EQ(view->world_seconds, start);
            ASSERT_EQ(view->table.slots[slot].time.seconds, start);
            dc_chunk_time_t remote_time;
            bool writing;
            ASSERT_TRUE(dc_level_offscreen_progress(view, first, view->camera_offscreen.generation,
                                                    &remote_time, &writing));
            ASSERT_TRUE(remote_time.known && remote_time.seconds == start && !writing);
            ASSERT_EQ(view->pending_steps, 1u);
            view->water_generations[slot] = pinned;
            ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
            ASSERT_EQ(view->table.slots[older_slot].time.seconds, older_start + 2. * elapsed);
            ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, older_slot, chunk, err, sizeof(err)));
            ASSERT_EQ(camera_stock(chunk, NULL, false), DC_FLUID_FULL);
            ASSERT_TRUE(chunk->cells[row+32u].fluid_mass < DC_FLUID_FULL);
        } else {
            ASSERT_TRUE(dc_level_view_step_timed(view, elapsed, err, sizeof(err)));
        }
        ASSERT_TRUE(dc_level_view_move(view, -1, 0));
        ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
        ASSERT_TRUE(!dc_level_offscreen_owns(view, first));
        ticks += 2u;
        ASSERT_EQ(view->world_seconds, 2. + ticks * (double)elapsed);
        ASSERT_TRUE(dc_gpu_set_tick_seconds(reference, elapsed) && dc_gpu_tick_step(reference, err, sizeof(err)) &&
                    dc_gpu_tick_step(reference, err, sizeof(err)));
        uint64_t stock[2] = {0};
        for (unsigned tile = 0; tile < 2u; ++tile) {
            uint32_t slot;
            ASSERT_TRUE(dc_chunk_table_find(&view->table, (dc_chunk_coord_t){first.x+tile, first.y}, &slot));
            ASSERT_EQ(view->table.slots[slot].time.seconds, view->world_seconds);
            ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
            stock[tile] = camera_stock(chunk, NULL, false);
            dc_chunk_t expected;
            ASSERT_TRUE(dc_gpu_download_chunk(reference, tile, &expected, err, sizeof(err)));
            if (gas) {
                fr_gas_checkpoint_chunk_t *actual=malloc(sizeof(*actual)),*native=malloc(sizeof(*native));
                ASSERT_TRUE(actual && native && fr_gas_gpu_export_checkpoint(view->gpu,slot,actual,err,sizeof(err)) &&
                    fr_gas_gpu_export_checkpoint(reference,tile,native,err,sizeof(err)));
                ASSERT_EQ(memcmp(actual->cells,native->cells,sizeof(actual->cells)),0);
                ASSERT_EQ(memcmp(actual->carrier_tail,native->carrier_tail,sizeof(actual->carrier_tail)),0);
                ASSERT_EQ(memcmp(actual->gates,native->gates,sizeof(actual->gates)),0);
                ASSERT_EQ(actual->clock.advanced_seconds,native->clock.advanced_seconds);
                if (tile) ASSERT_TRUE(native->cells[row][FR_GAS_EXHAUST]>0.f);
                free(native); free(actual);
            }
            if (grains) {
                ASSERT_EQ(chunk->particle_count,expected.particle_count);
                ASSERT_EQ(memcmp(chunk->particles,expected.particles,sizeof(chunk->particles)),0);
                if (tile && !cycle) ASSERT_EQ(expected.particle_count,1u);
                for (unsigned j=0;j<DC_MPM_PARTICLES_PER_CHUNK;j++) if (expected.particles[j].mass_fp) {
                    fr_reactive_record_t actual,native;
                    bool found=false;
                    ASSERT_TRUE(fr_gpu_reactive_read_particle(view->gpu,slot*DC_MPM_PARTICLES_PER_CHUNK+j,
                        &actual,&found,err,sizeof(err)) && found);
                    ASSERT_TRUE(fr_gpu_reactive_read_particle(reference,tile*DC_MPM_PARTICLES_PER_CHUNK+j,
                        &native,&found,err,sizeof(err)) && found);
                    ASSERT_EQ(memcmp(&actual,&native,sizeof(actual)),0);
                }
            }
            if (bulk_liquid) {
                ASSERT_TRUE(fr_bulk_phase_gpu_export_chunk(view->gpu, slot, phase, err, sizeof(err)) &&
                            fr_bulk_phase_gpu_export_chunk(reference, tile, expected_phase, err, sizeof(err)));
                ASSERT_EQ(memcmp(phase, expected_phase, sizeof(*phase)), 0);
            }
            if (heated) {
                ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(view->gpu, slot, heat, DC_CHUNK_CELLS, err, sizeof(err)) &&
                    fr_thermal_gpu_read_water_chunk(reference, tile, expected_heat, DC_CHUNK_CELLS, err, sizeof(err)));
                for (unsigned i = 0; i < DC_CHUNK_CELLS; ++i) ASSERT_TRUE(fabsf(heat[i] - expected_heat[i]) <= 1e-5f);
            }
            ASSERT_EQ(chunk->marker_count, expected.marker_count);
            qsort(chunk->markers, chunk->marker_count, sizeof(dc_marker_t), camera_marker_order);
            qsort(expected.markers, expected.marker_count, sizeof(dc_marker_t), camera_marker_order);
            ASSERT_EQ(memcmp(chunk->markers, expected.markers, chunk->marker_count * sizeof(dc_marker_t)), 0);
            for (unsigned i = 0; i < DC_CHUNK_CELLS; ++i) {
                ASSERT_EQ(chunk->cells[i].fluid_mass, expected.cells[i].fluid_mass);
                ASSERT_TRUE(fabsf(chunk->face_velocity[i].x - expected.face_velocity[i].x) <= 1e-5f);
                ASSERT_TRUE(fabsf(chunk->face_velocity[i].y - expected.face_velocity[i].y) <= 1e-5f);
            }
            ASSERT_TRUE(fr_renderer_download_chunk_colors(view->gpu, slot, &colors, err, sizeof(err)) &&
                fr_renderer_download_chunk_colors(reference, tile, &expected_colors, err, sizeof(err)));
            ASSERT_EQ(memcmp(colors.water, expected_colors.water, sizeof(colors.water)), 0);
        }
        ASSERT_TRUE(stock[1] > 0u);
        ASSERT_EQ(stock[0] + stock[1], 11u * (DC_FLUID_FULL/2u));
    }
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    dc_gpu_destroy(reference);
    free(chunk);
    free(phase);
    free(expected_phase);
    PASS();
}

static void test_camera_reopens_shared_gas_stock_and_pressure(void) {
    check_camera_water_returns(CAMERA_GAS|CAMERA_REOPEN);
}

static void test_camera_reopens_grains_after_they_cross_the_owner_face(void) {
    check_camera_water_returns(CAMERA_GRAINS|CAMERA_REOPEN);
}

static void test_camera_water_basin_returns_after_a_partial_shared_cycle(void) { check_camera_water_returns(0u); }
static void test_camera_heated_water_returns_with_its_energy(void) { check_camera_water_returns(CAMERA_HEAT); }
static void test_camera_bulk_liquids_return_with_their_momentum(void) { check_camera_water_returns(CAMERA_BULK); }
static void test_camera_reopens_its_confirmed_partial_cycle(void) { check_camera_water_returns(CAMERA_HEAT|CAMERA_BULK|CAMERA_REOPEN); }

static void test_two_evicted_sealed_rooms_return_at_the_world_clock(void) {
    char directory[] = "build/camera_two_rooms_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(view && chunk);
    dc_chunk_coord_t rooms[] = {view->origin, {view->origin.x + 1, view->origin.y + 1}};
    uint32_t center = 32u * DC_CHUNK_SIDE + 32u;
    for (unsigned i = 0; i < 2u; ++i) {
        uint32_t slot;
        uint64_t generation;
        camera_seal_room(chunk, rooms[i]);
        chunk->cells[center].fluid_mass = 14000u + i;
        ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, rooms[i], &slot, &generation) &&
            dc_gpu_upload_chunk(view->gpu, slot, chunk, err, sizeof(err)) &&
            dc_chunk_table_finish_load(&view->table, slot, generation) &&
            dc_chunk_table_mark_dirty(&view->table, slot));
        view->table.slots[slot].time = (dc_chunk_time_t){view->world_seconds, true};
    }
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)) &&
        dc_level_view_pan_pixels(view, 2 * VIEW_WIDTH, 0) &&
        dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(dc_level_offscreen_owns(view, rooms[0]));
    ASSERT_TRUE(dc_level_offscreen_owns(view, rooms[1]));
    ASSERT_TRUE(dc_level_view_step_timed(view, .0625f, err, sizeof(err)) &&
        dc_level_view_tick(view, err, sizeof(err)) &&
        dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0) &&
        dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    for (unsigned i = 0; i < 2u; ++i) {
        uint32_t slot;
        ASSERT_TRUE(!dc_level_offscreen_owns(view, rooms[i]) &&
            dc_chunk_table_find(&view->table, rooms[i], &slot) &&
            dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
        ASSERT_EQ(view->table.slots[slot].time.seconds, 2.0625);
        ASSERT_EQ(camera_stock(chunk, NULL, false), 14000u + i);
        ASSERT_TRUE(chunk->cells[center].fluid_mass < 14000u + i);
    }
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    free(chunk);
    PASS();
}

static void check_two_screen_camera_pan(bool moving_body, bool close_offscreen) {
    char directory[] = "build/camera_offscreen_pan_XXXXXX", err[256] = {0}, temporary[256];
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_bulk_phase_chunk_t *bulk = calloc(1, sizeof(*bulk));
    ASSERT_TRUE(view && chunk && bulk);
    const dc_chunk_coord_t coord = view->origin;
    camera_seal_room(chunk, coord);
    const uint32_t center = 32u * DC_CHUNK_SIDE + 32u;
    bulk->oil_fp[center] = 8000u;
    chunk->cells[center].fluid_mass = 14000u;
    uint32_t slot;
    uint64_t generation;
    ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, coord, &slot, &generation));
    ASSERT_TRUE(dc_gpu_upload_chunk(view->gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(view->gpu, slot, chunk, bulk, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_finish_load(&view->table, slot, generation));
    view->table.slots[slot].time = (dc_chunk_time_t){2., true};
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&view->table, slot));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));

    const uint32_t id = 507u;
    dc_gpu_world_body_t initial = {.chunk = coord, .body = {.id = id, .active = 1u, .width = 2u,
        .height = 3u, .x_fp = 2*DC_FLUID_FULL, .y_fp = 12*DC_FLUID_FULL, .vx_fp = DC_FLUID_FULL/4}};
    dc_gpu_world_body_t returned = {0}, observed;
    dc_gpu_body_motion_t motion = {0}, observed_motion;
    if (moving_body) {
        ASSERT_TRUE(dc_gpu_set_rigid_solver(view->gpu, true));
        ASSERT_TRUE(dc_gpu_spawn_world_body(view->gpu, initial, err, sizeof(err)) &&
                    dc_gpu_set_body_motion(view->gpu, id, .125f, .0625f, 0u, err, sizeof(err)));
        dc_gpu_world_body_t other = initial;
        other.chunk.x += 2;
        other.body.id = id + 1u;
        ASSERT_TRUE(dc_gpu_spawn_world_body(view->gpu, other, err, sizeof(err)));
    }
    snprintf(temporary, sizeof(temporary), "%s/chunk_%016" PRIx64 "_%016" PRIx64 ".bin.tmp",
        directory, (uint64_t)coord.x, (uint64_t)coord.y);
    ASSERT_EQ(mkdir(temporary, 0700), 0);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(!dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "Chunk worker load or save failed"));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
    ASSERT_TRUE(view->table.slots[slot].dirty && !dc_level_offscreen_owns(view, coord));
    if (moving_body) ASSERT_TRUE(dc_gpu_read_world_body(view->gpu, id, &observed, err, sizeof(err)));
    ASSERT_EQ(rmdir(temporary), 0);
    if (moving_body && !close_offscreen) {
        ASSERT_TRUE(schedule_saves(view, err, sizeof(err)));
        uint64_t deadline = SDL_GetTicks64() + 10000u;
        while (view->camera_offscreen.phase == DC_CAMERA_OFFSCREEN_SAVING && SDL_GetTicks64() < deadline) {
            ASSERT_TRUE(process_results(view, err, sizeof(err)));
            SDL_Delay(1);
        }
        ASSERT_EQ(view->camera_offscreen.phase, DC_CAMERA_OFFSCREEN_STORED);
        ASSERT_TRUE(dc_chunk_table_evict(&view->table, slot, UINT64_MAX));
        ASSERT_TRUE(dc_camera_offscreen_service(view, false, err, sizeof(err)));
        dc_stream_result_t failure = {.kind = DC_STREAM_FAILED, .coord = coord,
            .generation = view->camera_offscreen.generation};
        ASSERT_EQ(dc_level_offscreen_result(view, &failure, err, sizeof(err)), DC_LEVEL_OFFSCREEN_FAILED);
        ASSERT_EQ(view->camera_offscreen.phase, DC_CAMERA_OFFSCREEN_STORED);
        ASSERT_TRUE(dc_gpu_read_world_body(view->gpu, id, &observed, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(!dc_chunk_table_find(&view->table, coord, NULL));
    ASSERT_TRUE(dc_level_offscreen_owns(view, coord));
    if (moving_body) {
        ASSERT_TRUE(!dc_gpu_read_world_body(view->gpu, id, &observed, NULL, 0));
        ASSERT_TRUE(dc_gpu_read_world_body(view->camera_offscreen.workspace->gpu, id, &observed, err, sizeof(err)));
        ASSERT_EQ(memcmp(&initial, &observed, sizeof(initial)), 0);
        ASSERT_TRUE(dc_gpu_read_world_body(view->gpu, id + 1u, &observed, err, sizeof(err)));
        ASSERT_TRUE(!dc_gpu_read_world_body(view->camera_offscreen.workspace->gpu, id + 1u, &observed, NULL, 0));
        ASSERT_TRUE(!dc_gpu_spawn_world_body(view->gpu, initial, err, sizeof(err)));
        dc_gpu_world_body_t duplicate = initial;
        duplicate.body.id = id + 1u;
        ASSERT_TRUE(!dc_gpu_spawn_world_body(view->camera_offscreen.workspace->gpu, duplicate, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_step_timed(view, .03125f, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_step_timed(view, .03125f, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_EQ(view->world_seconds, 2.0625);
    if (close_offscreen) {
        dc_gpu_t *reference = NULL;
        ASSERT_TRUE(dc_gpu_create_shared(&reference, view->gpu, DC_CHUNK_SIDE, DC_CHUNK_SIDE,
            "build/shaders/pattern.comp.spv", err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_body_origin(reference, coord, err, sizeof(err)) &&
            dc_gpu_set_rigid_solver(reference, true) &&
            dc_gpu_upload_chunk(reference, 0u, chunk, err, sizeof(err)) &&
            dc_gpu_set_page(reference, 0u, 0u, 0u, err, sizeof(err)) &&
            fr_bulk_phase_gpu_import_chunk(reference, 0u, chunk, bulk, err, sizeof(err)) &&
            dc_gpu_spawn_world_body(reference, initial, err, sizeof(err)) &&
            dc_gpu_set_body_motion(reference, id, .125f, .0625f, 0u, err, sizeof(err)) &&
            dc_gpu_set_tick_seconds(reference, .03125f));
        ASSERT_TRUE(dc_gpu_tick_step(reference, err, sizeof(err)) && dc_gpu_tick_step(reference, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_world_body(reference, id, &returned, err, sizeof(err)) &&
            dc_gpu_read_body_motion(reference, id, &motion, err, sizeof(err)));
        dc_gpu_destroy(reference);
        ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
        view = camera_test_view(directory, true, err, sizeof(err));
        ASSERT_TRUE(view && dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
        ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
        ASSERT_EQ(view->table.slots[slot].time.seconds, view->world_seconds);
        ASSERT_TRUE(dc_gpu_read_world_body(view->gpu, id, &observed, err, sizeof(err)) &&
            dc_gpu_read_body_motion(view->gpu, id, &observed_motion, err, sizeof(err)));
        ASSERT_EQ(memcmp(&observed, &returned, sizeof(returned)), 0);
        ASSERT_EQ(observed_motion.angle, motion.angle);
        ASSERT_EQ(observed_motion.angular_velocity, motion.angular_velocity);
        dc_level_view_discard(view);
        free(bulk);
        free(chunk);
        PASS();
        return;
    }
    if (moving_body) {
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_world_body(view->camera_offscreen.workspace->gpu, id, &returned, err, sizeof(err)) &&
            dc_gpu_read_body_motion(view->camera_offscreen.workspace->gpu, id, &motion, err, sizeof(err)));
        ASSERT_TRUE(returned.body.x_fp > initial.body.x_fp && returned.body.vy_fp > 0 && motion.angle > .125f);
    }

    if (moving_body) {
        const char *draft = dc_world_save_directory(view->save, true);
        ASSERT_TRUE(draft);
        snprintf(temporary, sizeof(temporary), "%s/chunk_%016" PRIx64 "_%016" PRIx64 ".bin.tmp",
            draft, (uint64_t)coord.x, (uint64_t)coord.y);
    }
    ASSERT_EQ(mkdir(temporary, 0700), 0);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(!dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "Offscreen replacement write failed"));
    if (moving_body) {
        ASSERT_TRUE(!dc_gpu_read_world_body(view->gpu, id, &observed, NULL, 0));
        ASSERT_TRUE(dc_gpu_read_world_body(view->camera_offscreen.workspace->gpu, id, &observed, err, sizeof(err)));
        ASSERT_EQ(memcmp(&returned, &observed, sizeof(returned)), 0);
    }
    ASSERT_TRUE(!dc_chunk_table_find(&view->table, coord, NULL));
    ASSERT_TRUE(dc_level_offscreen_owns(view, coord));
    ASSERT_EQ(rmdir(temporary), 0);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
    ASSERT_TRUE(!dc_level_offscreen_owns(view, coord));
    if (moving_body) {
        ASSERT_TRUE(dc_gpu_read_world_body(view->gpu, id, &observed, err, sizeof(err)) &&
                    dc_gpu_read_body_motion(view->gpu, id, &observed_motion, err, sizeof(err)));
        ASSERT_EQ(memcmp(&returned, &observed, sizeof(returned)), 0);
        ASSERT_EQ(observed_motion.angle, motion.angle);
        ASSERT_EQ(observed_motion.angular_velocity, motion.angular_velocity);
    }
    ASSERT_TRUE(view->table.slots[slot].time.known && view->table.slots[slot].time.seconds == 2.0625);
    ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(fr_bulk_phase_gpu_export_chunk(view->gpu, slot, bulk, err, sizeof(err)));
    ASSERT_EQ(camera_stock(chunk, bulk, false), 14000u);
    ASSERT_EQ(camera_stock(chunk, bulk, true), 8000u);
    ASSERT_EQ(dc_gpu_validation_errors(view->gpu), 0u);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));

    view = camera_test_view(directory, true, err, sizeof(err));
    ASSERT_TRUE(view && dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    if (moving_body) {
        ASSERT_TRUE(dc_gpu_read_world_body(view->gpu, id, &observed, err, sizeof(err)) &&
                    dc_gpu_read_body_motion(view->gpu, id, &observed_motion, err, sizeof(err)));
        ASSERT_EQ(memcmp(&returned, &observed, sizeof(returned)), 0);
        ASSERT_EQ(observed_motion.angle, motion.angle);
        ASSERT_EQ(observed_motion.angular_velocity, motion.angular_velocity);
    }
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
    ASSERT_TRUE(view->table.slots[slot].time.known && view->table.slots[slot].time.seconds == 2.0625);
    ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(fr_bulk_phase_gpu_export_chunk(view->gpu, slot, bulk, err, sizeof(err)));
    ASSERT_EQ(camera_stock(chunk, bulk, false), 14000u);
    ASSERT_EQ(camera_stock(chunk, bulk, true), 8000u);
    ASSERT_EQ(dc_gpu_validation_errors(view->gpu), 0u);
    dc_level_view_discard(view);
    free(bulk);
    free(chunk);
    PASS();
}

static void test_two_screen_camera_pan_advances_and_reopens_its_evicted_owner(void) { check_two_screen_camera_pan(false, false); }
static void test_camera_transfers_moving_body_on_acknowledged_eviction_and_return(void) { check_two_screen_camera_pan(true, false); }
static void test_quit_saves_the_offscreen_moving_body_at_the_confirmed_world_clock(void) { check_two_screen_camera_pan(true, true); }

static void test_dormant_room_starts_offscreen_water_at_its_edit_time(void) {
    char directory[] = "build/dormant_edit_clock_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(view && chunk);
    const dc_chunk_coord_t coord = view->origin;
    camera_seal_room(chunk, coord);
    uint32_t slot;
    uint64_t request;
    ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, coord, &slot, &request));
    ASSERT_TRUE(dc_gpu_upload_chunk(view->gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_finish_load(&view->table, slot, request));
    view->table.slots[slot].time = (dc_chunk_time_t){2., true};
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&view->table, slot));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));

    ASSERT_TRUE(dc_level_view_pan_pixels(view, 2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(!dc_chunk_table_find(&view->table, coord, NULL));
    ASSERT_TRUE(!dc_level_offscreen_owns(view, coord));
    ASSERT_TRUE(dc_level_view_step_timed(view, .0625f, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_EQ(view->world_seconds, 2.0625);

    ASSERT_TRUE(dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
    ASSERT_TRUE(view->table.slots[slot].time.known && view->table.slots[slot].time.seconds == 2.0625);
    ASSERT_TRUE(dc_level_view_paint(view, 32u, 32u, 1u, DC_MATERIAL_WATER, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
    const uint64_t initial_water = camera_stock(chunk, NULL, false);
    ASSERT_TRUE(initial_water > 0);

    ASSERT_TRUE(dc_level_view_pan_pixels(view, 2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(!dc_chunk_table_find(&view->table, coord, NULL));
    ASSERT_TRUE(dc_level_offscreen_owns(view, coord));
    dc_chunk_time_t confirmed = {0};
    bool writing = false;
    uint64_t deadline = SDL_GetTicks64() + 25000u;
    do {
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_offscreen_progress(view, coord, view->camera_offscreen.generation,
            &confirmed, &writing));
        if (confirmed.known) break;
        SDL_Delay(1);
    } while (SDL_GetTicks64() < deadline);
    ASSERT_TRUE(confirmed.known && !writing && confirmed.seconds == 2.0625);
    dc_stream_result_t snapshot = {0};
    ASSERT_TRUE(dc_offscreen_snapshot(view->camera_offscreen.workspace, coord,
        view->camera_offscreen.generation, &snapshot, err, sizeof(err)));
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        ASSERT_EQ(snapshot.chunk->cells[i].fluid_mass, chunk->cells[i].fluid_mass);

    ASSERT_TRUE(dc_level_view_step_timed(view, .03125f, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
    ASSERT_TRUE(view->table.slots[slot].time.known && view->table.slots[slot].time.seconds == 2.09375);
    ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
    bool water_moved = false;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        water_moved |= snapshot.chunk->cells[i].fluid_mass != chunk->cells[i].fluid_mass;
    dc_stream_result_release(&snapshot);
    ASSERT_TRUE(water_moved);
    ASSERT_EQ(camera_stock(chunk, NULL, false), initial_water);
    ASSERT_EQ(dc_gpu_validation_errors(view->gpu), 0u);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));

    view = camera_test_view(directory, true, err, sizeof(err));
    ASSERT_TRUE(view && dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
    ASSERT_TRUE(view->table.slots[slot].time.known && view->table.slots[slot].time.seconds == 2.09375);
    ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
    ASSERT_EQ(camera_stock(chunk, NULL, false), initial_water);
    ASSERT_EQ(dc_gpu_validation_errors(view->gpu), 0u);
    dc_level_view_discard(view);
    free(chunk);
    PASS();
}

static void test_dormant_clock_uses_the_authoritative_structure_channels(void) {
    char directory[] = "build/dormant_owner_clock_XXXXXX", err[256] = {0}, path[256];
    ASSERT_TRUE(mkdtemp(directory));
    ASSERT_TRUE(snprintf(path, sizeof(path), "%s/%s", directory, DC_WORLD_BODY_RECORD) > 0);
    dc_level_view_t view = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_gas_checkpoint_chunk_t *gas = calloc(1, sizeof(*gas));
    ASSERT_TRUE(chunk && gas && structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_configure_atmosphere(&view, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_init(&view.table, DC_GPU_CHUNK_SLOTS));
    view.save = dc_world_save_open(directory, err, sizeof(err));
    view.stream = dc_stream_create(directory, 42, 4);
    ASSERT_TRUE(view.save && view.stream && dc_world_save_store_clock(view.save, 8., err, sizeof(err)));
    const dc_chunk_coord_t coords[] = {{-3, 2}, {-2, 2}};
    const uint32_t center = 32u * DC_CHUNK_SIDE + 32u;
    for (uint32_t i = 0; i < 2u; ++i) {
        camera_seal_room(chunk, coords[i]);
        chunk->cells[center].fluid_mass = i ? 14000u : 0u;
        uint32_t slot;
        uint64_t request;
        ASSERT_TRUE(dc_chunk_table_begin_load(&view.table, coords[i], &slot, &request));
        ASSERT_TRUE(dc_gpu_upload_chunk(view.gpu, slot, chunk, err, sizeof(err)));
        for (uint32_t cell = 0; cell < DC_CHUNK_CELLS; ++cell) {
            const float volume = chunk->cells[cell].material == DC_MATERIAL_STONE ? 0.f : 1.f;
            gas->cells[cell][FR_GAS_CHECKPOINT_VOLUME_WORD] = volume;
            gas->cells[cell][FR_GAS_CHECKPOINT_PREVIOUS_VOLUME_WORD] = volume;
        }
        ASSERT_TRUE(fr_gas_gpu_import_checkpoint(view.gpu, slot, gas, err, sizeof(err)));
        view.gas_admitted[slot] = true;
        ASSERT_TRUE(dc_chunk_table_finish_load(&view.table, slot, request));
        view.table.slots[slot].time = (dc_chunk_time_t){2. + i, true};
    }
    ASSERT_TRUE(fr_gpu_save_level_structure(view.gpu, path, &view.table, view.gas_admitted,
        err, sizeof(err)));
    for (uint32_t i = 0; i < 2u; ++i) {
        chunk->coord = coords[i];
        chunk->cells[center].fluid_mass = i ? 0u : 10000u;
        const dc_stream_result_t stale = {.coord = coords[i], .chunk = chunk, .time = {6., true},
            .kind = DC_STREAM_LOADED, .generation = 7u};
        const dc_chunk_owner_t wrong = {UINT64_MAX, 7u};
        ASSERT_TRUE(dc_stream_request_save_takeover(view.stream, &stale, wrong, 8u + i));
        dc_stream_result_t written = {0};
        ASSERT_TRUE(receive(view.stream, &written) && written.kind == DC_STREAM_SAVED);
        dc_stream_result_release(&written);
    }
    dc_gpu_destroy(view.gpu);
    dc_chunk_table_destroy(&view.table);

    view.gpu = NULL;
    ASSERT_TRUE(structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_configure_atmosphere(&view, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_init(&view.table, DC_GPU_CHUNK_SLOTS));
    ASSERT_TRUE(dc_world_save_load_clock(view.save, &view.world_seconds, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(view.gpu, path, err, sizeof(err)));
    for (uint32_t i = 0; i < 2u; ++i) {
        uint32_t slot;
        ASSERT_TRUE(publish_timed_chunk(&view, coords[i], &slot, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(view.gpu, slot, chunk, err, sizeof(err)));
        ASSERT_EQ(camera_stock(chunk, NULL, false), i ? 14000u : 0u);
        ASSERT_TRUE(view.table.slots[slot].time.known);
        ASSERT_EQ(view.table.slots[slot].time.seconds, i ? 3. : 8.);
    }
    ASSERT_EQ(dc_gpu_validation_errors(view.gpu), 0u);
    dc_stream_destroy(view.stream);
    dc_gpu_destroy(view.gpu);
    dc_chunk_table_destroy(&view.table);
    dc_world_save_close(view.save);
    free(gas);
    free(chunk);
    PASS();
}

#endif
