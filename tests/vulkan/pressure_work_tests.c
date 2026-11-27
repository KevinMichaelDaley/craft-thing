#define _POSIX_C_SOURCE 200809L
#define main fluid_reference_suite_main
#include "fluid_tests.c"
#undef main

static void test_pressure_work_is_empty_then_wakes_for_local_water(void) {
    dc_chunk_t left={0},right={0};char err[256]={0};
    dc_gpu_t *gpu=make_grid(&left,&right,err,sizeof(err));
    ASSERT_TRUE(gpu);
    bool available=gpu->pressure_work_mapped!=NULL;
    if(!available) {dc_gpu_destroy(gpu);ASSERT_TRUE(available);}
    ASSERT_TRUE(dc_gpu_fluid_step(gpu,err,sizeof(err)));
    const uint32_t *work=gpu->pressure_work_mapped;
    ASSERT_EQ(work[0],0u);ASSERT_EQ(work[1],0u);
    ASSERT_TRUE(dc_gpu_paint_material(gpu,110,20,1,DC_MATERIAL_WATER,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_fluid_step(gpu,err,sizeof(err)));
    ASSERT_TRUE(work[0]>0u&&work[0]<32u);
    ASSERT_EQ(work[1],work[0]);ASSERT_EQ(work[2],1u);ASSERT_EQ(work[3],1u);
    dc_gpu_destroy(gpu);PASS();
}
static void test_pressure_batches_have_exact_stencil_cache_storage(void) {
    dc_chunk_t left={0},right={0};char err[256]={0};
    dc_gpu_t *gpu=make_grid(&left,&right,err,sizeof(err));
    ASSERT_TRUE(gpu);
    VkMemoryRequirements requirements;
    ASSERT_TRUE(gpu->pressure_stencil_buffer!=VK_NULL_HANDLE);
    vkGetBufferMemoryRequirements(gpu->device,gpu->pressure_stencil_buffer,&requirements);
    VkDeviceSize required=4u*(VkDeviceSize)gpu->width*gpu->height*sizeof(uint32_t);
    bool enough=requirements.size>=required;
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(enough);PASS();
}
static void test_pressure_stencil_does_not_increase_mapped_memory(void) {
    dc_chunk_t left={0},right={0};char err[256]={0};dc_gpu_memory_stats_t cached,dense;
    ASSERT_EQ(unsetenv("DC_FLUID_VARIANTS"),0);
    dc_gpu_t *a=make_grid(&left,&right,err,sizeof(err));ASSERT_TRUE(a);
    ASSERT_EQ(setenv("DC_FLUID_VARIANTS","generic",1),0);
    dc_gpu_t *b=make_grid(&left,&right,err,sizeof(err));
    ASSERT_EQ(unsetenv("DC_FLUID_VARIANTS"),0);ASSERT_TRUE(b);
    ASSERT_TRUE(dc_gpu_memory_stats(a,&cached));ASSERT_TRUE(dc_gpu_memory_stats(b,&dense));
    uint64_t cached_mapped=cached.mapped_local_bytes+cached.mapped_system_bytes;
    uint64_t dense_mapped=dense.mapped_local_bytes+dense.mapped_system_bytes;
    printf("pressure cache mapped allocation %llu, dense %llu bytes\n",
        (unsigned long long)cached_mapped,(unsigned long long)dense_mapped);
    dc_gpu_destroy(a);dc_gpu_destroy(b);
    ASSERT_EQ(cached_mapped,dense_mapped);PASS();
}
static bool phase(dc_gpu_t *g,uint32_t mode) {
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if(vkResetCommandBuffer(g->command,0)!=VK_SUCCESS||
        vkBeginCommandBuffer(g->command,&begin)!=VK_SUCCESS) return false;
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        .srcAccessMask=VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    VkDependencyInfo dep={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(g->command,&dep);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
    g->fluid_step_scale=g->timed_fluid?(float)g->fluid_interval:1.f;
    dc_gpu_record_fluid_phase(g,mode);
    memory.srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memory.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    memory.dstStageMask=VK_PIPELINE_STAGE_2_HOST_BIT;
    memory.dstAccessMask=VK_ACCESS_2_HOST_READ_BIT;
    vkCmdPipelineBarrier2(g->command,&dep);
    if(vkEndCommandBuffer(g->command)!=VK_SUCCESS) return false;
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&g->command};
    return vkQueueSubmit(g->queue,1,&submit,VK_NULL_HANDLE)==VK_SUCCESS&&vkQueueWaitIdle(g->queue)==VK_SUCCESS;
}
static bool pressure_snapshot(dc_gpu_t *g,void *destination,char *err,uint32_t cap) {
    VkDeviceSize bytes=(VkDeviceSize)g->width*g->height*sizeof(float);
    VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void *mapped=NULL;
    if(!dc_gpu_make_mapped_buffer(g,bytes,&buffer,&memory,&mapped,err,cap)) return false;
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bool ok=vkResetCommandBuffer(g->command,0)==VK_SUCCESS&&vkBeginCommandBuffer(g->command,&begin)==VK_SUCCESS;
    if(ok) {
        VkMemoryBarrier2 barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT,.dstAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT};
        VkDependencyInfo dep={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&barrier};
        vkCmdPipelineBarrier2(g->command,&dep);
        VkBufferCopy copy={.size=bytes};vkCmdCopyBuffer(g->command,g->pressure_a_buffer,buffer,1,&copy);
        barrier.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;barrier.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask=VK_PIPELINE_STAGE_2_HOST_BIT;barrier.dstAccessMask=VK_ACCESS_2_HOST_READ_BIT;
        vkCmdPipelineBarrier2(g->command,&dep);
        ok=vkEndCommandBuffer(g->command)==VK_SUCCESS;
        VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&g->command};
        if(ok) ok=vkQueueSubmit(g->queue,1,&submit,VK_NULL_HANDLE)==VK_SUCCESS&&vkQueueWaitIdle(g->queue)==VK_SUCCESS;
        if(ok) memcpy(destination,mapped,(size_t)bytes);
    }
    vkUnmapMemory(g->device,memory);vkDestroyBuffer(g->device,buffer,NULL);vkFreeMemory(g->device,memory,NULL);
    return ok;
}
static void test_water_inserted_between_pressure_batches_matches_dense_solver(void) {
    dc_chunk_t left={0},right={0};char err[256]={0};
    left.cells[8*64+8].fluid_mass=DC_FLUID_FULL;
    ASSERT_EQ(unsetenv("DC_FLUID_VARIANTS"),0);
    dc_gpu_t *sparse=make_grid(&left,&right,err,sizeof(err));ASSERT_TRUE(sparse);
    ASSERT_EQ(setenv("DC_FLUID_VARIANTS","generic",1),0);
    dc_gpu_t *dense=make_grid(&left,&right,err,sizeof(err));
    ASSERT_EQ(unsetenv("DC_FLUID_VARIANTS"),0);ASSERT_TRUE(dense);
    ASSERT_TRUE(phase(sparse,0));ASSERT_TRUE(phase(dense,0));
    size_t bytes=(size_t)sparse->width*sparse->height*sizeof(float);
    void *pressure_a=malloc(bytes),*pressure_b=malloc(bytes);ASSERT_TRUE(pressure_a&&pressure_b);
    ASSERT_TRUE(pressure_snapshot(sparse,pressure_a,err,sizeof(err)));
    ASSERT_TRUE(pressure_snapshot(dense,pressure_b,err,sizeof(err)));
    ASSERT_EQ(memcmp(pressure_a,pressure_b,bytes),0);
    ASSERT_TRUE(dc_gpu_paint_material(sparse,110,40,1,DC_MATERIAL_WATER,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(dense,110,40,1,DC_MATERIAL_WATER,err,sizeof(err)));
    for(uint32_t mode=1;mode<6;mode++) {
        ASSERT_TRUE(phase(sparse,mode));ASSERT_TRUE(phase(dense,mode));
        ASSERT_TRUE(pressure_snapshot(sparse,pressure_a,err,sizeof(err)));
        ASSERT_TRUE(pressure_snapshot(dense,pressure_b,err,sizeof(err)));
        ASSERT_EQ(memcmp(pressure_a,pressure_b,bytes),0);
        if(mode==1) {
            const uint32_t *work=sparse->pressure_work_mapped;bool found=false;
            for(uint32_t i=0;i<work[0];i++) found|=work[4+i]==22u;
            ASSERT_TRUE(found);
        }
        for(uint32_t slot=0;slot<2;slot++) {
            dc_chunk_t a,b;
            ASSERT_TRUE(dc_gpu_download_chunk(sparse,slot,&a,err,sizeof(err)));
            ASSERT_TRUE(dc_gpu_download_chunk(dense,slot,&b,err,sizeof(err)));
            ASSERT_EQ(memcmp(a.cells,b.cells,sizeof(a.cells)),0);
            ASSERT_EQ(memcmp(a.face_velocity,b.face_velocity,sizeof(a.face_velocity)),0);
        }
    }
    free(pressure_a);free(pressure_b);dc_gpu_destroy(sparse);dc_gpu_destroy(dense);PASS();
}
static void test_cached_stencil_matches_dense_with_partial_faces_and_mixed_terrain(void) {
    dc_chunk_t left={0},right={0};char err[256]={0};uint32_t seed=0x684126u;
    for(unsigned slot=0;slot<2;slot++) {
        dc_chunk_t *chunk=slot?&right:&left;
        for(unsigned i=0;i<DC_CHUNK_CELLS;i++) {
            seed=seed*1664525u+1013904223u;
            unsigned material=seed%11u;
            chunk->cells[i].material=material==0?DC_MATERIAL_STONE:material==1?DC_MATERIAL_DIRT:
                material==2?DC_MATERIAL_GRAVEL:material==3?DC_MATERIAL_LEAF_DEBRIS:DC_MATERIAL_AIR;
            chunk->cells[i].fluid_mass=(seed>>7u)%65537u;
        }
    }
    ASSERT_EQ(unsetenv("DC_FLUID_VARIANTS"),0);
    dc_gpu_t *sparse=make_grid(&left,&right,err,sizeof(err));ASSERT_TRUE(sparse);
    ASSERT_EQ(setenv("DC_FLUID_VARIANTS","generic",1),0);
    dc_gpu_t *dense=make_grid(&left,&right,err,sizeof(err));
    ASSERT_EQ(unsetenv("DC_FLUID_VARIANTS"),0);ASSERT_TRUE(dense);
    size_t cells=(size_t)sparse->width*sparse->height,bytes=cells*sizeof(float);
    void *a=malloc(bytes),*b=malloc(bytes);ASSERT_TRUE(a&&b);
    dc_gpu_t *grids[]={sparse,dense};
    for(unsigned grid=0;grid<2;grid++) {
        uint32_t *occupancy=grids[grid]->occupancy_mapped;
        dc_face_velocity_t *velocity=grids[grid]->velocity_mapped;
        for(size_t i=0;i<cells;i++) {
            occupancy[2*cells+i]=i%13u==0u?65536u:((uint32_t)i*499u)%65536u;
            occupancy[3*cells+i]=((uint32_t)i*733u)%65537u;
            occupancy[4*cells+i]=((uint32_t)i*977u)%65537u;
            velocity[i]=(dc_face_velocity_t){((int)(i%19u)-9)/16.f,((int)(i%23u)-11)/16.f};
        }
    }
    for(uint32_t mode=0;mode<6;mode++) {
        ASSERT_TRUE(phase(sparse,mode));ASSERT_TRUE(phase(dense,mode));
        ASSERT_TRUE(pressure_snapshot(sparse,a,err,sizeof(err)));
        ASSERT_TRUE(pressure_snapshot(dense,b,err,sizeof(err)));
        if(memcmp(a,b,bytes)!=0) {
            unsigned mismatches=0;
            for(size_t i=0;i<cells;i++) if(((uint32_t *)a)[i]!=((uint32_t *)b)[i]) {
                if(mismatches<4) printf("stencil mode%u cell%zu cached %08x %g dense %08x %g\n",
                    mode,i,((uint32_t *)a)[i],((float *)a)[i],((uint32_t *)b)[i],((float *)b)[i]);
                mismatches++;
            }
            printf("stencil pressure mismatches%u\n",mismatches);
        }
        ASSERT_EQ(memcmp(a,b,bytes),0);
        for(uint32_t slot=0;slot<2;slot++) {
            dc_chunk_t ca,cb;
            ASSERT_TRUE(dc_gpu_download_chunk(sparse,slot,&ca,err,sizeof(err)));
            ASSERT_TRUE(dc_gpu_download_chunk(dense,slot,&cb,err,sizeof(err)));
            ASSERT_EQ(memcmp(ca.cells,cb.cells,sizeof(ca.cells)),0);
            ASSERT_EQ(memcmp(ca.face_velocity,cb.face_velocity,sizeof(ca.face_velocity)),0);
        }
    }
    free(a);free(b);dc_gpu_destroy(sparse);dc_gpu_destroy(dense);PASS();
}
static dc_gpu_t *traced_workspace;
static VkPipeline traced_pipeline;
static unsigned expected_lines[2], transport_modes, wrong_lines;
static const dc_gpu_t *traced_pressure;
static unsigned tile_scans, stencil_builds;

void __real_vkCmdDispatchIndirect(VkCommandBuffer, VkBuffer, VkDeviceSize);
void __wrap_vkCmdDispatchIndirect(VkCommandBuffer command, VkBuffer buffer, VkDeviceSize offset) {
    if (traced_pressure && traced_pipeline == traced_pressure->projection_phase_pipelines[6])
        ++stencil_builds;
    __real_vkCmdDispatchIndirect(command, buffer, offset);
}
static uint32_t expected_slots, pressure_dispatches, mass_dispatches, wrong_dispatches;

void __real_vkCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
void __wrap_vkCmdBindPipeline(VkCommandBuffer command, VkPipelineBindPoint point, VkPipeline pipeline) {
    traced_pipeline = pipeline;
    __real_vkCmdBindPipeline(command, point, pipeline);
}

void __real_vkCmdDispatch(VkCommandBuffer, uint32_t, uint32_t, uint32_t);
void __wrap_vkCmdDispatch(VkCommandBuffer command, uint32_t x, uint32_t y, uint32_t z) {
    if (traced_pressure && traced_pipeline == traced_pressure->pressure_work_pipelines[1])
        ++tile_scans;
    if (traced_workspace) {
        dc_gpu_t *g = traced_workspace;
        for (unsigned mode = 0; mode < 4; ++mode) {
            if (traced_pipeline != g->fluid_transport_pipelines[mode]) continue;
            transport_modes |= 1u << mode;
            wrong_lines += x != expected_lines[mode / 2] || y != 1 || z != 1;
        }
        bool pressure = traced_pipeline == g->projection_pipeline;
        for (unsigned i = 0; i < 7; ++i)
            pressure |= traced_pipeline == g->projection_phase_pipelines[i];
        bool mass = traced_pipeline == g->fluid_pipeline;
        for (unsigned i = 4; i <= 10; ++i)
            mass |= traced_pipeline == g->fluid_phase_pipelines[i];
        if (pressure || mass) {
            wrong_dispatches += z != expected_slots;
            pressure_dispatches += pressure;
            mass_dispatches += mass;
        }
    }
    __real_vkCmdDispatch(command, x, y, z);
}

static void test_sparse_workspace_pages_match_full_slot_dispatch(void) {
    enum { WIDTH = 150, HEIGHT = 143, PIXELS = WIDTH * HEIGHT };
    const char *shader = "build/shaders/pattern.comp.spv";
    char err[256] = {0};
    dc_gpu_t *resident = NULL, *workspace = NULL;
    ASSERT_TRUE(dc_gpu_create(&resident, WIDTH, HEIGHT, shader, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_create_shared(&workspace, resident, WIDTH, HEIGHT, shader, err, sizeof(err)));
    dc_gpu_t *owners[] = {resident, workspace};
    dc_chunk_t *chunks = calloc(2, sizeof(*chunks));
    float *pressure = malloc(2 * PIXELS * sizeof(*pressure));
    ASSERT_TRUE(chunks && pressure);
    for (unsigned round = 0; round < 4; ++round) {
        unsigned slot = round ? 5 : 0;
        unsigned x = round == 1 ? 2 : 0, y = round >= 2 ? 2 : 0;
        memset(chunks, 0, sizeof(*chunks));
        chunks->cells[8 * DC_CHUNK_SIDE + 8].fluid_mass = DC_FLUID_FULL;
        chunks->face_velocity[8 * DC_CHUNK_SIDE + 8].x = .5f;
        chunks->coord = (dc_chunk_coord_t){x, y};
        chunks->marker_count = 1;
        chunks->markers[0] = (dc_marker_t){8 * DC_FLUID_FULL, 8 * DC_FLUID_FULL, 33 + slot, DC_MARKER_INSIDE};
        for (unsigned owner = 0; owner < 2; ++owner) {
            dc_gpu_t *g = owners[owner];
            if (round < 3)
                ASSERT_TRUE(dc_gpu_upload_chunk(g, slot, chunks, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_set_page(g, x, y, round == 3 ? UINT32_MAX : slot, err, sizeof(err)));
            if (g == workspace) {
                expected_slots = round == 1 || round == 2 ? 6 : 1;
                expected_lines[0] = round == 1 ? WIDTH : DC_CHUNK_SIDE;
                expected_lines[1] = round == 2 ? HEIGHT : DC_CHUNK_SIDE;
                transport_modes = wrong_lines = 0;
                pressure_dispatches = mass_dispatches = wrong_dispatches = 0;
                traced_workspace = g;
            }
            /* Two full cycles return cached command selection to the same parity. */
            for (unsigned cycle = 0; cycle < 2; ++cycle)
                ASSERT_TRUE(dc_gpu_fluid_step(g, err, sizeof(err)));
            traced_workspace = NULL;
            if (g == workspace) {
                printf("Mapped slot extent %u: %u pressure, %u mass, %u wrong-depth dispatches\n",
                       expected_slots, pressure_dispatches, mass_dispatches, wrong_dispatches);
                ASSERT_TRUE(pressure_dispatches && mass_dispatches);
                ASSERT_EQ(wrong_dispatches, 0u);
                printf("Water transport: %u columns, %u rows, mode mask %#x, %u mismatches\n",
                       expected_lines[0], expected_lines[1], transport_modes, wrong_lines);
                ASSERT_EQ(transport_modes, 15u);
                ASSERT_EQ(wrong_lines, 0u);
            }
            ASSERT_TRUE(pressure_snapshot(g, pressure + owner * PIXELS, err, sizeof(err)));
        }
        ASSERT_EQ(resident->fluid_tick, workspace->fluid_tick);
        ASSERT_EQ(resident->fluid_tick, (round + 1) * 2);
        ASSERT_EQ(memcmp(pressure, pressure + PIXELS, PIXELS * sizeof(*pressure)), 0);
        for (unsigned page = 0; page < (round == 1 || round == 2 ? 2u : 1u); ++page) {
            for (unsigned owner = 0; owner < 2; ++owner)
                ASSERT_TRUE(dc_gpu_download_chunk(owners[owner], page * 5, chunks + owner, err, sizeof(err)));
            ASSERT_EQ(memcmp(chunks[0].cells, chunks[1].cells, sizeof(chunks[0].cells)), 0);
            ASSERT_EQ(memcmp(chunks[0].face_velocity, chunks[1].face_velocity, sizeof(chunks[0].face_velocity)), 0);
            ASSERT_EQ(chunks[0].marker_count, chunks[1].marker_count);
            for (unsigned owner = 0; owner < 2; ++owner)
                qsort(chunks[owner].markers, chunks[owner].marker_count, sizeof(dc_marker_t), marker_compare);
            ASSERT_EQ(memcmp(chunks[0].markers, chunks[1].markers,
                             chunks[0].marker_count * sizeof(*chunks[0].markers)), 0);
        }
    }
    free(pressure);
    free(chunks);
    ASSERT_EQ(dc_gpu_validation_errors(resident), 0u);
    dc_gpu_destroy(workspace);
    dc_gpu_destroy(resident);
    PASS();
}

static void test_scheduled_pressure_runs_match_independent_phases(void) {
    enum { PIXELS = 2 * DC_CHUNK_CELLS };
    char err[256] = {0};
    dc_chunk_t *chunks = calloc(2, sizeof(*chunks));
    float *pressure = calloc(2 * PIXELS, sizeof(*pressure));
    ASSERT_TRUE(chunks && pressure);
    chunks[0].cells[16 * DC_CHUNK_SIDE + 12].fluid_mass = DC_FLUID_FULL;
    chunks[0].face_velocity[16 * DC_CHUNK_SIDE + 12].x = .75f;
    chunks[1].coord.x = 1;
    dc_gpu_t *run = make_grid(chunks, chunks + 1, err, sizeof(err));
    dc_gpu_t *ref = make_grid(chunks, chunks + 1, err, sizeof(err));
    ASSERT_TRUE(run && ref);
    ASSERT_TRUE(dc_gpu_set_fluid_interval(run, 6) && dc_gpu_set_fluid_interval(ref, 6));
    ASSERT_TRUE(run->pressure_work_enabled);
    /* The last tick records fresh and continued phase one at identical parity. */
    const unsigned lengths[] = {2, 2, 2, 6, 6, 1, 13};
    const unsigned expected_uncached[] = {1, 1, 0};
    bool counts_match = true;
    unsigned next = 0;
    for (unsigned tick = 0; tick < sizeof(lengths) / sizeof(*lengths); ++tick) {
        if (tick == 1) {
            ASSERT_TRUE(pressure[16 * run->width + 12] != 0.f);
            /* Leave residual pressure at the drained cell and introduce a new wet tile. */
            memset(chunks, 0, sizeof(*chunks));
            dc_gpu_t *owners[] = {run, ref};
            for (unsigned i = 0; i < 2; ++i) {
                ASSERT_TRUE(dc_gpu_upload_chunk(owners[i], 0, chunks, err, sizeof(err)));
                ASSERT_TRUE(dc_gpu_set_page(owners[i], 1, 0, UINT32_MAX, err, sizeof(err)));
                ASSERT_TRUE(dc_gpu_upload_chunk(owners[i], 2, chunks + 1, err, sizeof(err)));
                ASSERT_TRUE(dc_gpu_set_page(owners[i], 1, 0, 2, err, sizeof(err)));
                ASSERT_TRUE(dc_gpu_paint_material(owners[i], 90, 28, 1, DC_MATERIAL_WATER, err, sizeof(err)));
            }
        }
        float seconds = lengths[tick] / 60.f;
        ASSERT_TRUE(dc_gpu_set_tick_seconds(run, seconds) && dc_gpu_set_tick_seconds(ref, seconds));
        run->solver_cache_disabled = tick < 3;
        tile_scans = stencil_builds = 0;
        traced_pressure = run;
        ASSERT_TRUE(dc_gpu_tick_step(run, err, sizeof(err)));
        traced_pressure = NULL;
        if (tick < 3) {
            printf("Pressure run from phase %u over %u phases: %u scans, %u stencils\n",
                   next, lengths[tick], tile_scans, stencil_builds);
            counts_match &= tile_scans == expected_uncached[tick] && stencil_builds == expected_uncached[tick];
        }
        for (unsigned i = 0; i < lengths[tick]; ++i) {
            ASSERT_TRUE(phase(ref, next));
            next = (next + 1) % 6;
        }
        ASSERT_EQ(run->fluid_tick, ref->fluid_tick);
        ASSERT_TRUE(pressure_snapshot(run, pressure, err, sizeof(err)));
        ASSERT_TRUE(pressure_snapshot(ref, pressure + PIXELS, err, sizeof(err)));
        ASSERT_EQ(memcmp(pressure, pressure + PIXELS, PIXELS * sizeof(*pressure)), 0);
        for (unsigned page = 0; page < 2; ++page) {
            unsigned slot = page && tick ? 2 : page;
            ASSERT_TRUE(dc_gpu_download_chunk(run, slot, chunks, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_download_chunk(ref, slot, chunks + 1, err, sizeof(err)));
            ASSERT_EQ(memcmp(chunks[0].cells, chunks[1].cells, sizeof(chunks[0].cells)), 0);
            ASSERT_EQ(memcmp(chunks[0].face_velocity, chunks[1].face_velocity, sizeof(chunks[0].face_velocity)), 0);
            ASSERT_EQ(chunks[0].marker_count, chunks[1].marker_count);
            for (unsigned i = 0; i < 2; ++i)
                qsort(chunks[i].markers, chunks[i].marker_count, sizeof(dc_marker_t), marker_compare);
            ASSERT_EQ(memcmp(chunks[0].markers, chunks[1].markers,
                             chunks[0].marker_count * sizeof(dc_marker_t)), 0);
        }
    }
    ASSERT_EQ(dc_gpu_validation_errors(run), 0u);
    ASSERT_EQ(dc_gpu_validation_errors(ref), 0u);
    dc_gpu_destroy(run);
    dc_gpu_destroy(ref);
    free(chunks);
    free(pressure);
    ASSERT_TRUE(counts_match);
    PASS();
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--runs")) {
        RUN(test_scheduled_pressure_runs_match_independent_phases);
        return g_fail != 0;
    }
    RUN(test_sparse_workspace_pages_match_full_slot_dispatch);
    if (argc == 2 && !strcmp(argv[1], "--workspace")) return g_fail != 0;
    RUN(test_scheduled_pressure_runs_match_independent_phases);
    RUN(test_pressure_batches_have_exact_stencil_cache_storage);
    RUN(test_pressure_stencil_does_not_increase_mapped_memory);
    RUN(test_pressure_work_is_empty_then_wakes_for_local_water);
    RUN(test_water_inserted_between_pressure_batches_matches_dense_solver);
    RUN(test_cached_stencil_matches_dense_with_partial_faces_and_mixed_terrain);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail!=0;
}
