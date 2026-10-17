#include <stdio.h>
#include <string.h>

#include "ferrum/assets/prop_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "renderer_internal.h"

static bool placement_error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

bool fr_gpu_place_terrain_prop(dc_gpu_t *g, const fr_prop_asset_t *asset,
                               uint32_t frame, uint32_t x, uint32_t y,
                               char *err, uint32_t cap) {
    fr_prop_asset_t validated;
    if (!g || !asset || !fr_prop_asset_open(&validated,asset->bytes,asset->size,err,cap) ||
        frame>=validated.frame_count || x>=g->width || y>=g->height ||
        validated.width>g->width-x || validated.height>g->height-y)
        return placement_error(err,cap,"Invalid or nonresident terrain prop extent");
    if (vkQueueWaitIdle(g->queue)!=VK_SUCCESS || !fr_renderer_enable(g,err,cap)) return false;
    uint64_t local_before=g->mapped_local_bytes, system_before=g->mapped_system_bytes;
    uint32_t count=validated.width*validated.height;
    VkDeviceSize bytes=32+(VkDeviceSize)count*72;
    VkBuffer buffer=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    void *mapped=NULL;
    fr_renderer_compute_t compute={0};
    bool okay=dc_gpu_make_mapped_buffer(g,bytes,&buffer,&memory,&mapped,err,cap);
    if (!okay) goto done;
    memset(mapped,0,32);
    memcpy((uint8_t*)mapped+32,validated.bytes+validated.data_offset+(size_t)frame*count*72,(size_t)count*72);
    VkDeviceSize atlas=(VkDeviceSize)DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS;
    VkDescriptorBufferInfo bindings[11]={
        {buffer,0,bytes}, {g->chunk_buffer,0,atlas*sizeof(dc_cell_t)},
        {g->page_buffer,0,(VkDeviceSize)g->page_width*g->page_height*4},
        {g->particle_buffer,0,atlas*2*sizeof(dc_mpm_particle_t)},
        {g->particle_count_buffer,0,DC_GPU_CHUNK_SLOTS*4},
        {g->slot_seed_buffer,0,DC_GPU_CHUNK_SLOTS*4},
        {VK_NULL_HANDLE,0,0},{VK_NULL_HANDLE,0,0},{VK_NULL_HANDLE,0,0},
        {g->body_buffer,0,dc_gpu_body_storage_bytes(g)},
        {g->occupancy_buffer,0,(VkDeviceSize)g->width*g->height*20}};
    okay=fr_renderer_authored_bindings(g,bindings+6) &&
        fr_renderer_compute_init(g,&compute,bindings,11,32,
                                 "build/shaders/prop_place.comp.spv",err,cap) && fr_renderer_begin(g);
    if (!okay) goto done;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_HOST_WRITE_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    uint32_t push[8]={g->width,g->height,x,y,validated.width,validated.height,0,
        (uint32_t)(sizeof(dc_gpu_body_record_t)*DC_GPU_BODY_CAPACITY/4)+dc_gpu_structural_word_offset(g)};
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute.pipeline_layout,0,1,&compute.descriptor,0,NULL);
    for (uint32_t phase=0;phase<2;phase++) {
        push[6]=phase;
        vkCmdPushConstants(g->command,compute.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdDispatch(g->command,(count+127)/128,1,1);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT);
    }
    okay=fr_renderer_submit(g);
    if (okay && *(uint32_t*)mapped!=0)
        okay=placement_error(err,cap,"Terrain prop preflight rejected role, unknown cell or body overlap");
    if (okay) {
        g->body_refresh_pending=true;
        g->fluid_snapshot_valid=false;
        dc_gpu_structural_data(g)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD]=1;
    }
done:
    fr_renderer_compute_destroy(g,&compute);
    if (mapped) vkUnmapMemory(g->device,memory);
    if (buffer) vkDestroyBuffer(g->device,buffer,NULL);
    if (memory) vkFreeMemory(g->device,memory,NULL);
    g->mapped_local_bytes=local_before;
    g->mapped_system_bytes=system_before;
    return okay;
}
