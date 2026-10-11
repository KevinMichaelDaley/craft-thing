#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "gpu_internal.h"

_Static_assert(sizeof(fr_structural_gpu_graph_t) == DC_STRUCTURAL_GRAPH_WORDS*4u, "Structural graph ABI");
_Static_assert(sizeof(fr_structural_gpu_node_state_t) == DC_STRUCTURAL_NODE_STATE_WORDS*4u, "Structural node ABI");
static bool error(char *err,uint32_t cap,const char *s) {
    if(err && cap) snprintf(err,cap,"%s",s);
    return false;
}
static int node_index(const fr_structural_gpu_graph_t *g,uint32_t id) {
    for(uint32_t i=0;i<g->node_count;i++) if(g->nodes[i].id==id) return (int)i;
    return -1;
}
static bool delta(const fr_structural_gpu_node_t *a,const fr_structural_gpu_node_t *b,double out[2]) {
    if(a->chunk.x==INT64_MIN || a->chunk.x==INT64_MAX || a->chunk.y==INT64_MIN || a->chunk.y==INT64_MAX)
        return false;
    if(b->chunk.x<a->chunk.x-1 || b->chunk.x>a->chunk.x+1 ||
       b->chunk.y<a->chunk.y-1 || b->chunk.y>a->chunk.y+1) return false;
    out[0]=(double)(b->chunk.x-a->chunk.x)*64+(double)(b->x_fp-a->x_fp)/65536;
    out[1]=(double)(b->chunk.y-a->chunk.y)*64+(double)(b->y_fp-a->y_fp)/65536;
    return true;
}
static bool finite_values(const float *values,unsigned count) {
    for(unsigned i=0;i<count;i++) if(!isfinite(values[i]) || values[i]<0) return false;
    return true;
}
static bool validate(const fr_structural_gpu_graph_t *g) {
    if(!g || g->node_count>FR_STRUCTURAL_NODE_CAPACITY || g->member_count>FR_STRUCTURAL_MEMBER_CAPACITY ||
       g->joint_count>FR_STRUCTURAL_JOINT_CAPACITY || g->anchor_count>FR_STRUCTURAL_ANCHOR_CAPACITY) return false;
    for(unsigned i=0;i<12;i++) if(g->reserved[i]) return false;
    for(uint32_t i=0;i<g->node_count;i++) {
        const fr_structural_gpu_node_t *n=&g->nodes[i];
        if(!n->id || !n->generation || n->x_fp<0 || n->y_fp<0 || n->x_fp>=64<<16 || n->y_fp>=64<<16) return false;
        for(unsigned k=0;k<4;k++) if(n->reserved[k]) return false;
        for(uint32_t j=0;j<i;j++) if(g->nodes[j].id==n->id) return false;
    }
    for(uint32_t i=0;i<g->member_count;i++) {
        const fr_structural_gpu_member_t *m=&g->members[i];
        int a=node_index(g,m->a),b=node_index(g,m->b); double d[2];
        if(!m->id || m->id>=FR_STRUCTURAL_BODY_ID_BASE || !m->generation || a<0 || b<0 || a==b ||
           m->material>FR_STRUCTURAL_WOOD || !m->depth_fp || m->depth_fp>16u<<16 || m->flags&~1u || m->reserved ||
           !delta(&g->nodes[a],&g->nodes[b],d)) return false;
        double length=hypot(d[0],d[1]); if(length<1.0/65536 || length>16) return false;
        if(m->flags&FR_STRUCTURAL_MARK_TERRAIN) {
            if(m->material!=FR_STRUCTURAL_STONE || d[1]!=0 || d[0]<=0 ||
               (m->depth_fp&65535u) || (g->nodes[a].x_fp&65535) ||
               ((g->nodes[a].y_fp-(int32_t)(m->depth_fp/2))&65535) || d[0]!=floor(d[0])) return false;
        }
        for(uint32_t j=0;j<i;j++) if(g->members[j].id==m->id) return false;
    }
    for(uint32_t i=0;i<g->joint_count;i++) {
        const fr_structural_gpu_joint_t *j=&g->joints[i];double d[2];
        int a=node_index(g,j->a),b=node_index(g,j->b);
        if(!j->id || !j->generation || j->kind>FR_JOINT_MOMENT || a<0 || b<0 || a==b ||
           !finite_values(&j->normal_stiffness,8) || !delta(&g->nodes[a],&g->nodes[b],d) || hypot(d[0],d[1])>2)
            return false;
        for(unsigned k=0;k<3;k++) if(j->reserved[k]) return false;
        for(uint32_t k=0;k<i;k++) if(g->joints[k].id==j->id) return false;
    }
    for(uint32_t i=0;i<g->anchor_count;i++) {
        const fr_structural_gpu_anchor_t *a=&g->anchors[i];
        if(!a->id || !a->generation || node_index(g,a->node)<0 || a->kind>FR_JOINT_MOMENT ||
           !a->width || !a->height || a->width>8 || a->height>8 || a->x>=64 || a->y>=64 ||
           a->terrain_material!=DC_MATERIAL_STONE || !finite_values(&a->normal_stiffness,8)) return false;
        for(unsigned k=0;k<3;k++) if(a->reserved[k]) return false;
        for(uint32_t j=0;j<i;j++) if(g->anchors[j].id==a->id) return false;
    }
    return true;
}
void dc_gpu_structural_track_chunk(dc_gpu_t *gpu,uint32_t slot,dc_chunk_coord_t coord) {
    if(!gpu || !gpu->body_mapped || slot>=DC_GPU_CHUNK_SLOTS) return;
    uint32_t *p=dc_gpu_structural_data(gpu)+DC_STRUCTURAL_HEADER_WORDS+slot*8u;
    memcpy(p,&coord,sizeof(coord));++p[4];p[5]=1;
}
bool dc_gpu_structural_pipeline_init(dc_gpu_t *gpu,char *err,uint32_t cap) {
    VkShaderModule shader;
    if(!dc_gpu_load_shader_module(gpu,"build/shaders/structural.comp.spv",&shader,err,cap)) return false;
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
                .module=shader,.pName="main"},.layout=gpu->pipeline_layout};
    VkResult r=vkCreateComputePipelines(gpu->device,VK_NULL_HANDLE,1,&info,NULL,&gpu->structural_pipeline);
    vkDestroyShaderModule(gpu->device,shader,NULL);
    return r==VK_SUCCESS || error(err,cap,"Cannot create structural pipeline");
}
static void dispatch(dc_gpu_t *gpu,uint32_t mode) {
    VkMemoryBarrier2 b={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask=VK_ACCESS_2_HOST_WRITE_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    VkDependencyInfo dep={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&b};
    vkCmdPipelineBarrier2(gpu->command,&dep);
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->structural_pipeline);
    vkCmdBindDescriptorSets(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->pipeline_layout,0,1,&gpu->descriptor,0,NULL);
    uint64_t x=(uint64_t)gpu->body_origin.x,y=(uint64_t)gpu->body_origin.y;
    uint32_t push[8]={gpu->width,gpu->height,mode,gpu->body_count,(uint32_t)x,(uint32_t)(x>>32),(uint32_t)y,(uint32_t)(y>>32)};
    vkCmdPushConstants(gpu->command,gpu->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(gpu->command,1,1,1);
    b.srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;b.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    b.dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT;
    b.dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT;
    vkCmdPipelineBarrier2(gpu->command,&dep);
}
void dc_gpu_record_structural(dc_gpu_t *gpu) {
    if(gpu->structural_enabled) dispatch(gpu,1);
}
static bool commit(dc_gpu_t *gpu,char *err,uint32_t cap) {
    if(vkResetCommandBuffer(gpu->command,0)!=VK_SUCCESS) return error(err,cap,"Cannot reset structural transaction");
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if(vkBeginCommandBuffer(gpu->command,&begin)!=VK_SUCCESS) return false;
    dispatch(gpu,0);dispatch(gpu,1);
    if(vkEndCommandBuffer(gpu->command)!=VK_SUCCESS) return false;
    VkSubmitInfo info={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&gpu->command};
    return vkQueueSubmit(gpu->queue,1,&info,VK_NULL_HANDLE)==VK_SUCCESS && vkQueueWaitIdle(gpu->queue)==VK_SUCCESS;
}
static bool spawn_member(dc_gpu_t *gpu,const fr_structural_gpu_graph_t *g,uint32_t index,char *err,uint32_t cap) {
    const fr_structural_gpu_member_t *m=&g->members[index];
    const fr_structural_gpu_node_t *a=&g->nodes[node_index(g,m->a)],*b=&g->nodes[node_index(g,m->b)];
    double d[2];if(!delta(a,b,d)) return false;
    double length=hypot(d[0],d[1]),depth=(double)m->depth_fp/65536;
    double px=(double)a->x_fp/65536+d[0]/2-length/2;
    double py=(double)a->y_fp/65536+d[1]/2-depth/2;
    int dx=(int)floor(px/64),dy=(int)floor(py/64);
    if((dx<0 && a->chunk.x<INT64_MIN-dx) || (dx>0 && a->chunk.x>INT64_MAX-dx) ||
       (dy<0 && a->chunk.y<INT64_MIN-dy) || (dy>0 && a->chunk.y>INT64_MAX-dy)) return false;
    dc_gpu_world_body_t w={.chunk={a->chunk.x+dx,a->chunk.y+dy},.body={
        .x_fp=(int32_t)llround((px-dx*64)*65536),.y_fp=(int32_t)llround((py-dy*64)*65536),
        .width=(uint32_t)ceil(length),.height=(uint32_t)ceil(depth),.id=FR_STRUCTURAL_BODY_ID_BASE+m->id,.active=1}};
    int32_t lf=(int32_t)llround(length*65536),df=(int32_t)m->depth_fp;
    dc_gpu_body_shape_t shape={.count=4,.material=m->material==FR_STRUCTURAL_WOOD ? DC_GPU_BODY_WOOD : DC_GPU_BODY_STONE,
        .vertices={{0,0},{lf,0},{lf,df},{0,df}}};
    if(!dc_gpu_spawn_convex_body(gpu,w,&shape,err,cap) ||
       !dc_gpu_set_body_motion(gpu,w.body.id,(float)atan2(d[1],d[0]),0,DC_GPU_BODY_KINEMATIC,err,cap)) return false;
    for(uint32_t i=0;i<gpu->body_count;i++) if(gpu->body_ids[i]==w.body.id) {
        dc_gpu_body_record_t *r=(dc_gpu_body_record_t *)gpu->body_mapped+i;
        r->body.active=0;r->reserved[0]=DC_GPU_STRUCTURAL_TAG;r->reserved[1]=index+1;
        fr_structural_gpu_graph_t *staged=(void *)(dc_gpu_structural_data(gpu)+dc_gpu_structural_graph_offset()+DC_STRUCTURAL_GRAPH_WORDS);
        staged->members[index].reserved=i;return true;
    }
    return false;
}
bool fr_gpu_place_structure(dc_gpu_t *gpu,const fr_structural_gpu_graph_t *g,char *err,uint32_t cap) {
    if(!gpu || !validate(g)) return error(err,cap,"Invalid structural graph or unsupported geometry");
    dc_gpu_body_record_t backup[DC_GPU_BODY_CAPACITY];uint32_t ids[DC_GPU_BODY_CAPACITY],count=gpu->body_count;
    memcpy(backup,gpu->body_mapped,sizeof(backup));memcpy(ids,gpu->body_ids,sizeof(ids));
    unsigned free_count=0;
    for(unsigned i=0;i<DC_GPU_BODY_CAPACITY;i++) if(!ids[i] || backup[i].reserved[0]==DC_GPU_STRUCTURAL_TAG) free_count++;
    if(g->member_count>free_count) return error(err,cap,"Structural collision pool full");
    uint32_t *data=dc_gpu_structural_data(gpu);
    memcpy(data+dc_gpu_structural_graph_offset()+DC_STRUCTURAL_GRAPH_WORDS,g,sizeof(*g));
    for(unsigned i=0;i<DC_GPU_BODY_CAPACITY;i++) if(backup[i].reserved[0]==DC_GPU_STRUCTURAL_TAG) {
        memset((dc_gpu_body_record_t *)gpu->body_mapped+i,0,sizeof(backup[i]));gpu->body_ids[i]=0;
    }
    bool okay=true;
    for(uint32_t i=0;okay && i<g->member_count;i++) okay=spawn_member(gpu,g,i,err,cap);
    if(okay) okay=commit(gpu,err,cap) && data[2]==1;
    if(!okay) {
        memcpy(gpu->body_mapped,backup,sizeof(backup));memcpy(gpu->body_ids,ids,sizeof(ids));gpu->body_count=count;
        return error(err,cap,"Structural transaction rejected; ownership unchanged");
    }
    gpu->structural_enabled=g->member_count!=0;
    gpu->body_refresh_pending=true;
    return dc_gpu_set_body_origin(gpu,gpu->body_origin,err,cap);
}
bool fr_gpu_read_structure_node(dc_gpu_t *gpu,uint32_t id,fr_structural_gpu_node_state_t *out) {
    if(!gpu || !id || !out) return false;
    uint32_t *data=dc_gpu_structural_data(gpu),off=dc_gpu_structural_graph_offset();
    const fr_structural_gpu_graph_t *g=(void *)(data+off);int index=node_index(g,id);
    if(index<0) return false;
    memcpy(out,data+off+2*DC_STRUCTURAL_GRAPH_WORDS+(uint32_t)index*DC_STRUCTURAL_NODE_STATE_WORDS,sizeof(*out));
    return true;
}
