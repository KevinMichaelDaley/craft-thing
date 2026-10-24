#define main pixel_fixture_main
#include "pixel_component_tests.c"
#undef main

static bool finish_command(dc_gpu_t *g) {
    if(vkEndCommandBuffer(g->command)!=VK_SUCCESS) return false;
    VkFence fence;VkFenceCreateInfo info={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if(vkCreateFence(g->device,&info,NULL,&fence)!=VK_SUCCESS) return false;
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&g->command};
    VkResult status=vkQueueSubmit(g->queue,1,&submit,fence);
    if(status==VK_SUCCESS) status=vkWaitForFences(g->device,1,&fence,VK_TRUE,UINT64_C(1000000000));
    if(status!=VK_SUCCESS) {printf("raster fence status%d\n",status);return false;}
    vkDestroyFence(g->device,fence,NULL);return true;
}
static bool copy_activity(dc_gpu_t *g,VkBuffer staging,bool upload) {
    if(vkResetCommandBuffer(g->command,0)!=VK_SUCCESS) return false;
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if(vkBeginCommandBuffer(g->command,&begin)!=VK_SUCCESS) return false;
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=upload?VK_PIPELINE_STAGE_2_HOST_BIT:VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask=upload?VK_ACCESS_2_HOST_WRITE_BIT:VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT,.dstAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT};
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(g->command,&dependency);
    VkBufferCopy region={.size=6u*sizeof(uint32_t)};
    vkCmdCopyBuffer(g->command,upload?staging:g->mpm_activity_buffer,upload?g->mpm_activity_buffer:staging,1,&region);
    memory.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;memory.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;
    memory.dstStageMask=upload?VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT:VK_PIPELINE_STAGE_2_HOST_BIT;
    memory.dstAccessMask=upload?VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT:VK_ACCESS_2_HOST_READ_BIT;
    vkCmdPipelineBarrier2(g->command,&dependency);return finish_command(g);
}
static bool raster_once(dc_gpu_t *g,VkPipeline pipeline) {
    if(vkResetCommandBuffer(g->command,0)!=VK_SUCCESS) return false;
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if(vkBeginCommandBuffer(g->command,&begin)!=VK_SUCCESS) return false;
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
    uint32_t push[8]={g->width,g->height,129,dc_gpu_runtime_body_count(g)};
    vkCmdPushConstants(g->command,g->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,(g->width+15)/16,(g->height+15)/16,1);
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_HOST_BIT,.dstAccessMask=VK_ACCESS_2_HOST_READ_BIT};
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(g->command,&dependency);
    return finish_command(g);
}
static bool equal_draw_images(dc_gpu_t *g,char *err) {
    VkPipeline simple=g->simple_draw_pipeline,pixel=g->pixel_draw_pipeline;
    size_t bytes=(size_t)g->width*g->height*sizeof(uint32_t);
    void *reference=malloc(bytes);if(!reference) return false;
    bool equal=true;
    for(unsigned overlay=0;overlay<=DC_GPU_OVERLAY_STRUCTURAL_STRESS&&equal;overlay++) {
        equal=dc_gpu_set_overlay(g,(dc_gpu_overlay_t)overlay);
        g->simple_draw_pipeline=g->pixel_draw_pipeline=VK_NULL_HANDLE;
        equal=equal&&dc_gpu_render_chunks(g,err,256);
        if(equal) memcpy(reference,g->mapped,bytes);
        g->simple_draw_pipeline=simple;g->pixel_draw_pipeline=pixel;
        for(unsigned frame=0;frame<4&&equal;frame++) {
            equal=dc_gpu_render_chunks(g,err,256)&&memcmp(reference,g->mapped,bytes)==0;
            if(!equal) printf("draw mismatch overlay%u paused frame%u\n",overlay,frame);
        }
    }
    free(reference);return equal;
}
static int contact_order(const void *a,const void *b) {return memcmp(a,b,sizeof(dc_gpu_contact_t));}
static bool equal_pixel_contacts(dc_gpu_t *g,char *err) {
    VkPipeline typed=g->contact_pixel_pipeline;
    dc_gpu_contact_t *reference=malloc(DC_GPU_CONTACT_CAPACITY*sizeof(*reference));
    dc_gpu_contact_t *actual=malloc(DC_GPU_CONTACT_CAPACITY*sizeof(*actual));
    if(!reference||!actual) {free(reference);free(actual);return false;}
    dc_gpu_body_record_t *b=g->body_mapped;
    for(unsigned slot=0;slot<2;slot++) {
        if(!dc_gpu_set_body_motion(g,b[slot].body.id,slot==0?.31f:-.27f,0,0,err,256)) return false;
        b[slot].chunk_x[0]=b[slot].chunk_x[1]=0;
        b[slot].chunk_y[0]=1;b[slot].chunk_y[1]=0;
        b[slot].local_x_fp=(slot==0?30:45)<<16;
        b[slot].local_y_fp=(slot==0?16:15)<<16;
    }
    g->contact_pixel_pipeline=VK_NULL_HANDLE;
    bool equal=dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,256);
    dc_gpu_contact_stats_t expected={0};
    equal=equal&&dc_gpu_read_contacts(g,&expected,reference,DC_GPU_CONTACT_CAPACITY,err,256);
    g->contact_pixel_pipeline=typed;
    bool body=false,terrain=false,grain=false;
    uint32_t counters[3];memcpy(counters,dc_gpu_contact_data(g)+9,sizeof(counters));
    for(unsigned i=0;i<expected.count;i++) {body|=reference[i].kind==0;terrain|=reference[i].kind==1;grain|=reference[i].kind==3;}
    equal=equal&&expected.overflow==0&&body&&terrain&&grain;
    qsort(reference,expected.count,sizeof(*reference),contact_order);
    for(unsigned frame=0;frame<4&&equal;frame++) {
        dc_gpu_contact_stats_t stats;
        equal=dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,256)&&
            dc_gpu_read_contacts(g,&stats,actual,DC_GPU_CONTACT_CAPACITY,err,256);
        if(equal) {
            qsort(actual,stats.count,sizeof(*actual),contact_order);
            equal=memcmp(&expected,&stats,sizeof(stats))==0&&memcmp(reference,actual,stats.count*sizeof(*actual))==0&&
                memcmp(counters,dc_gpu_contact_data(g)+9,sizeof(counters))==0;
        }
    }
    printf("concave typed contacts: %u constraints body%d terrain%d grain%d exact%d\n",expected.count,body,terrain,grain,equal);
    free(reference);free(actual);return equal;
}
static VkPipeline raster_team_pipeline(dc_gpu_t *g,const char *path) {
    VkShaderModule module;char err[256];
    if(!dc_gpu_load_shader_module(g,path,&module,err,sizeof(err))) return VK_NULL_HANDLE;
    uint32_t values[]={1,0};VkSpecializationMapEntry entries[]={{0,0,4},{1,4,4}};
    VkSpecializationInfo spec={2,entries,sizeof(values),values};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=g->pipeline_layout};
    VkPipeline pipeline=VK_NULL_HANDLE;
    printf("raster team pipeline begin %s\n",path);fflush(stdout);
    vkCreateComputePipelines(g->device,VK_NULL_HANDLE,1,&info,NULL,&pipeline);
    printf("raster team pipeline end %s\n",path);fflush(stdout);
    vkDestroyShaderModule(g->device,module,NULL);return pipeline;
}
static bool bounded_raster_teams_match_frozen_scan(dc_gpu_t *g,char *err) {
    for(unsigned i=0;i<83;i++) {
        unsigned x=3u+i%10u*10u,y=4u+i/10u*11u;
        dc_gpu_world_body_t b={.body={.id=100u+i,.active=1,.width=6,.height=6,
            .x_fp=(int32_t)((x%64u)*65536u+(i*997u)%65536u),
            .y_fp=(int32_t)((y%64u)*65536u+(i*619u)%65536u)},.chunk={x/64u,y/64u}};
        if(!dc_gpu_spawn_world_body(g,b,err,256)||
           !dc_gpu_set_body_motion(g,b.body.id,.013f*(i%7u),0,DC_GPU_BODY_KINEMATIC,err,256)) {
            printf("raster fixture body%u: %s\n",i,err);return false;
        }
    }
    if(!dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,256)) return false;
    VkBuffer counter;VkDeviceMemory memory;uint32_t *count;
    if(!dc_gpu_make_mapped_buffer(g,16,&counter,&memory,(void **)&count,err,256)) return false;
    VkDescriptorBufferInfo buffer={counter,0,16};
    VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=g->descriptor,
        .dstBinding=31,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffer};
    vkUpdateDescriptorSets(g->device,1,&write,0,NULL);
    size_t bytes=(size_t)g->width*g->height*20u;void *reference=malloc(bytes);
    if(!reference) return false;
    const char *paths[]={"build/diagnostics/raster_team_reference.spv","build/diagnostics/raster_team_profile.spv"};
    uint32_t barriers[2]={0};bool exact=true;
    for(unsigned variant=0;variant<2;variant++) {
        VkPipeline pipeline=raster_team_pipeline(g,paths[variant]);
        if(!pipeline) return false;
        *count=0;
        if(!raster_once(g,pipeline)) return false;
        barriers[variant]=*count;
        if(!variant) memcpy(reference,g->occupancy_mapped,bytes);
        else exact=memcmp(reference,g->occupancy_mapped,bytes)==0;
        vkDestroyPipeline(g->device,pipeline,NULL);
    }
    uint32_t teams=((g->width+15u)/16u)*((g->height+15u)/16u);
    uint32_t batches=(dc_gpu_runtime_body_count(g)+63u)/64u,limit=4u*teams*batches;
    printf("raster candidate team barriers%u -> %u (limit%u), %u slots, exact five planes%d\n",
        barriers[0],barriers[1],limit,dc_gpu_runtime_body_count(g),exact);
    buffer=(VkDescriptorBufferInfo){g->fluid_previous_buffer,0,(VkDeviceSize)g->width*g->height*4u};
    vkUpdateDescriptorSets(g->device,1,&write,0,NULL);
    free(reference);vkUnmapMemory(g->device,memory);vkDestroyBuffer(g->device,counter,NULL);vkFreeMemory(g->device,memory,NULL);
    return exact&&barriers[0]==15u*teams*batches&&barriers[1]<=limit&&batches>1u;
}
static void test_two_rotated_concave_bodies_use_exact_pixel_raster(void) {
    char err[256];dc_gpu_t *g=NULL;
    ASSERT_TRUE(dc_gpu_create(&g,123,117,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned cy=0;cy<2;cy++) for(unsigned cx=0;cx<2;cx++) {
        *chunk=(dc_chunk_t){.coord={cx,cy}};
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            unsigned px=cx*64+x,py=cy*64+y,material=DC_MATERIAL_AIR;
            if(px>=12&&px<43&&py>=14&&py<43&&(px<18||py>=37)) material=px<18?DC_MATERIAL_WOOD:DC_MATERIAL_STONE;
            if(px>=70&&px<104&&py>=52&&py<87&&(px<76||px>=98||py>=80)) material=px<94?DC_MATERIAL_DENSE_WOOD:DC_MATERIAL_WOOD;
            if(py==110&&px<123) material=DC_MATERIAL_STONE;
            if(py==105&&px>=30&&px<85) material=DC_MATERIAL_SAND;
            chunk->cells[y*64+x].material=material;
        }
        unsigned slot=cy*2+cx;
        dc_chunk_seed_particles(chunk);
        ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,cx,cy,slot,err,sizeof(err)));
    }
    free(chunk);ASSERT_TRUE(dc_gpu_enable_automatic_components(g));ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    uint32_t count;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,NULL,0,&count,err,sizeof(err)));ASSERT_EQ(count,324u);
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000002u,NULL,0,&count,err,sizeof(err)));ASSERT_EQ(count,574u);
    ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000001u,.31f,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000002u,-.27f,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    VkBuffer staging;VkDeviceMemory staging_memory;uint32_t *activity;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(g,6u*sizeof(uint32_t),&staging,&staging_memory,(void **)&activity,err,sizeof(err)));
    /* A missed reset remains bounded even if accidentally consumed indirectly. */
    for(unsigned i=0;i<6;i++) activity[i]=i%3==0?7:1;
    ASSERT_TRUE(copy_activity(g,staging,true));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(copy_activity(g,staging,false));
    bool selected=activity[0]==0&&activity[1]==1&&activity[2]==1&&activity[3]==8&&activity[4]==8&&activity[5]==1;
    printf("runtime raster dispatch: %u,%u,%u / %u,%u,%u\n",activity[0],activity[1],activity[2],activity[3],activity[4],activity[5]);
    vkUnmapMemory(g->device,staging_memory);vkDestroyBuffer(g->device,staging,NULL);vkFreeMemory(g->device,staging_memory,NULL);
    if(!selected) {dc_gpu_destroy(g);ASSERT_TRUE(selected);}
    ASSERT_TRUE(g->rigid_pixel_pipeline!=VK_NULL_HANDLE);
    ASSERT_TRUE(g->simple_draw_pipeline!=VK_NULL_HANDLE&&g->pixel_draw_pipeline!=VK_NULL_HANDLE);
    ASSERT_TRUE(g->contact_pixel_pipeline!=VK_NULL_HANDLE);
    size_t bytes=(size_t)g->width*g->height*5*sizeof(uint32_t);void *reference=malloc(bytes);ASSERT_TRUE(reference);
    ASSERT_TRUE(raster_once(g,g->rigid_pipeline));memcpy(reference,g->occupancy_mapped,bytes);
    for(unsigned frame=0;frame<4;frame++) {
        ASSERT_TRUE(raster_once(g,g->rigid_pixel_pipeline));
        ASSERT_EQ(memcmp(reference,g->occupancy_mapped,bytes),0);
    }
    printf("two concave shapes: all five occupancy/volume/face planes identical across four paused frames\n");
    free(reference);
    ASSERT_TRUE(equal_draw_images(g,err));
    ASSERT_TRUE(dc_gpu_set_viewport(g,7,9,91,73));
    ASSERT_TRUE(equal_draw_images(g,err));
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(g,6u*sizeof(uint32_t),&staging,&staging_memory,(void **)&activity,err,sizeof(err)));
    ASSERT_TRUE(copy_activity(g,staging,false));
    ASSERT_EQ(activity[0],0u);ASSERT_EQ(activity[3],6u);ASSERT_EQ(activity[4],5u);
    dc_gpu_body_record_t *legacy=(dc_gpu_body_record_t *)g->body_mapped+63;
    *legacy=(dc_gpu_body_record_t){.body={.id=777,.active=1,.width=8,.height=8},.visible=0};
    ASSERT_TRUE(equal_draw_images(g,err));
    ASSERT_TRUE(copy_activity(g,staging,false));
    ASSERT_EQ(activity[0],6u);ASSERT_EQ(activity[1],5u);ASSERT_EQ(activity[3],0u);
    vkUnmapMemory(g->device,staging_memory);vkDestroyBuffer(g->device,staging,NULL);vkFreeMemory(g->device,staging_memory,NULL);
    printf("all overlays and cropped views draw byte-identical paused images; hidden legacy selects generic\n");
    memset(legacy,0,sizeof(*legacy));
    ASSERT_TRUE(equal_pixel_contacts(g,err));
    ASSERT_TRUE(bounded_raster_teams_match_frozen_scan(g,err));
    dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_two_rotated_concave_bodies_use_exact_pixel_raster);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
