#define main carve_fixture_main
#include "body_carve_tests.c"
#undef main

static bool carve_work_pipeline(dc_gpu_t *g,char *err,uint32_t cap) {
    VkShaderModule module=VK_NULL_HANDLE;
    if(!dc_gpu_load_shader_module(g,"build/diagnostics/carve_work_profile.spv",&module,err,cap)) return false;
    uint32_t value=6u;VkSpecializationMapEntry entry={0,0,4};
    VkSpecializationInfo spec={1,&entry,sizeof(value),&value};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=g->pipeline_layout};
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkResult result=vkCreateComputePipelines(g->device,VK_NULL_HANDLE,1,&info,NULL,&pipeline);
    vkDestroyShaderModule(g->device,module,NULL);
    if(result!=VK_SUCCESS) return false;
    vkDestroyPipeline(g->device,g->structural_pipelines[6],NULL);g->structural_pipelines[6]=pipeline;return true;
}
static bool carve_colors_match(dc_gpu_t *g,const dc_gpu_body_pixel_t *pixels,unsigned count) {
    dc_gpu_body_record_t *b=dc_gpu_body_record(g,0);
    const uint32_t *shape=(const uint32_t *)g->body_mapped+b->compound_reserved;
    for(unsigned i=0;i<count;i++) {
        unsigned x=pixels[i].x,y=pixels[i].y,cell=(y%16u)*16u+x%16u;
        unsigned tile=(y/16u)*shape[2]+x/16u;
        uint32_t word=b->compound_reserved+shape[8u+tile]+1u+cell/8u;
        const float *color=dc_gpu_shape_colors(g,word)+(cell%8u)*4u;
        if(color[0]!=(x+1u)%16u/16.f||color[1]!=(y+1u)%16u/16.f||color[2]!=.375f||color[3]!=1.f) return false;
    }
    return true;
}
static void run_carve_work_case(bool material_queries) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));
    ASSERT_TRUE(chunk&&colors);
    ASSERT_TRUE(dc_gpu_create(&g,192,192,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(unsigned cy=0;cy<3;cy++) for(unsigned cx=0;cx<3;cx++) {
        memset(chunk,0,sizeof(*chunk));memset(colors,0,sizeof(*colors));chunk->coord=(dc_chunk_coord_t){cx,cy};
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            unsigned wx=cx*64+x,wy=cy*64+y,index=y*64+x;
            if(wx==0||wx>=191||wy==0||wy>=191) continue;
            chunk->cells[index].material=DC_MATERIAL_WOOD;
            colors->terrain[index][0]=(wx%16u)/16.f;colors->terrain[index][1]=(wy%16u)/16.f;
            colors->terrain[index][2]=.375f;colors->terrain[index][3]=1.f;
        }
        ASSERT_TRUE(dc_gpu_upload_chunk(g,cy*3+cx,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,cx,cy,cy*3+cx,err,sizeof(err)));
        ASSERT_TRUE(fr_renderer_upload_chunk_colors(g,cy*3+cx,colors,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    for(unsigned step=0;step<4;step++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_runtime_prepare_fragments(g,err,sizeof(err)));
    ASSERT_TRUE(carve_work_pipeline(g,err,sizeof(err)));
    VkBuffer buffer;VkDeviceMemory memory;uint32_t *counts;
    VkDeviceSize profile_bytes=(VkDeviceSize)g->width*g->height*sizeof(uint32_t);
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(g,profile_bytes,&buffer,&memory,(void **)&counts,err,sizeof(err)));
    memset(counts,0,(size_t)profile_bytes);
    VkDescriptorBufferInfo binding={buffer,0,profile_bytes};
    VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=g->descriptor,
        .dstBinding=31,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&binding};
    vkUpdateDescriptorSets(g->device,1,&write,0,NULL);g->descriptor_revision++;
    ASSERT_TRUE(dc_gpu_paint_material(g,96,96,4,DC_MATERIAL_AIR,err,sizeof(err)));
    unsigned neighbor_checks=counts[1],count=0,after_count=0;
    dc_gpu_body_pixel_t *before=calloc(190u*190u,sizeof(*before)),*after=calloc(190u*190u,sizeof(*after));
    ASSERT_TRUE(before&&after);
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,before,190u*190u,&count,err,sizeof(err)));
    ASSERT_EQ(count,36051u);ASSERT_TRUE(carve_colors_match(g,before,count));
    dc_gpu_body_motion_t motion;ASSERT_TRUE(dc_gpu_read_body_motion(g,0x80000001u,&motion,err,sizeof(err)));
    ASSERT_TRUE(fabsf(motion.mass-(float)count*.6f)<.01f);
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    memset(counts,0,16);
    ASSERT_TRUE(dc_gpu_paint_material(g,96,96,4,DC_MATERIAL_AIR,err,sizeof(err)));
    unsigned repeated_checks=counts[0];
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,after,190u*190u,&after_count,err,sizeof(err)));
    ASSERT_EQ(after_count,count);ASSERT_EQ(memcmp(before,after,count*sizeof(*before)),0);
    ASSERT_TRUE(carve_colors_match(g,after,after_count));ASSERT_TRUE(!g->body_refresh_pending);
    double hole_x=190.*190.*95.,hole_y=hole_x;
    for(unsigned i=0;i<count;i++) {hole_x-=before[i].x+.5;hole_y-=before[i].y+.5;}
    hole_x/=(36100u-count);hole_y/=(36100u-count);
    const float angle=.37f;
    ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000001u,angle,0,1,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    dc_gpu_body_record_t *body=dc_gpu_body_record(g,0);
    float dx=(float)hole_x-body->center[0],dy=(float)hole_y-body->center[1];
    uint32_t x=(uint32_t)floorf(body->body.x_fp/65536.f+body->center[0]+cosf(angle)*dx-sinf(angle)*dy);
    uint32_t y=(uint32_t)floorf(body->body.y_fp/65536.f+body->center[1]+sinf(angle)*dx+cosf(angle)*dy);
    memset(counts,0,16);
    ASSERT_TRUE(dc_gpu_paint_material(g,x,y,2,DC_MATERIAL_AIR,err,sizeof(err)));
    unsigned rotated_checks=counts[0];
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,after,190u*190u,&after_count,err,sizeof(err)));
    ASSERT_EQ(after_count,count);ASSERT_EQ(memcmp(before,after,count*sizeof(*before)),0);
    ASSERT_TRUE(carve_colors_match(g,after,after_count));ASSERT_TRUE(!g->body_refresh_pending);
    printf("carve neighbor material queries%u, empty-hole inspections%u rotated%u (limit512)\n",
        neighbor_checks,repeated_checks,rotated_checks);
    vkUnmapMemory(g->device,memory);vkDestroyBuffer(g->device,buffer,NULL);vkFreeMemory(g->device,memory,NULL);
    dc_gpu_destroy(g);free(chunk);free(colors);free(before);free(after);
    if(material_queries) {ASSERT_EQ(neighbor_checks,0u);}
    else {ASSERT_TRUE(repeated_checks<=512u);ASSERT_TRUE(rotated_checks<=512u);}
    PASS();
}
static void test_carve_reuses_classification_without_neighbor_material_queries(void) {run_carve_work_case(true);}
static void test_empty_hole_brush_work_is_bounded_after_rotation(void) {run_carve_work_case(false);}
int main(void) {
    RUN(test_carve_reuses_classification_without_neighbor_material_queries);
    RUN(test_empty_hole_brush_work_is_bounded_after_rotation);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
