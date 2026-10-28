#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/physics/revolute_gpu.h"
#include "revolute_gpu_internal.h"
#include "runtime_body_internal.h"

enum { JOINT_WORDS=16, JOINT_WORK_HEADER=96 };
struct fr_revolute_gpu {dc_gpu_address_buffer_t records;uint32_t span,capacity;};
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
uint32_t fr_gpu_revolute_span(const dc_gpu_t *g) {return g&&g->revolute?g->revolute->span:0;}
void fr_gpu_revolute_bind_work(dc_gpu_t *g,uint32_t *h) {
    struct fr_revolute_gpu *r=g->revolute;
    VkDeviceAddress address=r?r->records.address:0;
    h[JOINT_WORK_HEADER]=(uint32_t)address;h[JOINT_WORK_HEADER+1u]=(uint32_t)(address>>32u);
    h[JOINT_WORK_HEADER+2u]=r?r->span:0;
}
void fr_gpu_revolute_destroy(dc_gpu_t *g) {
    if(!g||!g->revolute) return;
    dc_gpu_address_buffer_destroy(g,&g->revolute->records);
    free(g->revolute);g->revolute=NULL;
}
static bool reserve(dc_gpu_t *g,uint32_t required,char *err,uint32_t cap) {
    struct fr_revolute_gpu *r=g->revolute;
    if(required<=r->capacity) return true;
    uint64_t next=r->capacity?r->capacity:8u;
    while(next<required) next*=2u;
    if(next>UINT32_MAX/JOINT_WORDS) return error(err,cap,"Joint resource size overflow");
    dc_gpu_address_buffer_t pending={0};
    if(!dc_gpu_address_buffer_create(g,next*JOINT_WORDS*4u,true,&pending,err,cap)) return false;
    if(r->span) {
        if(!fr_renderer_begin(g)) {dc_gpu_address_buffer_destroy(g,&pending);return error(err,cap,"Cannot begin joint growth");}
        VkBufferCopy copy={.size=(VkDeviceSize)r->span*JOINT_WORDS*4u};
        vkCmdCopyBuffer(g->command,r->records.buffer,pending.buffer,1,&copy);
        if(!fr_renderer_submit(g)) {dc_gpu_address_buffer_destroy(g,&pending);return error(err,cap,"Cannot copy joint storage");}
    }
    dc_gpu_address_buffer_destroy(g,&r->records);r->records=pending;r->capacity=(uint32_t)next;
    return true;
}
static uint32_t find_body(dc_gpu_t *g,uint32_t id) {
    for(uint32_t i=0;i<dc_gpu_body_capacity(g);i++) {
        dc_gpu_body_record_t *b=dc_gpu_body_record(g,i);
        if(b->body.id==id&&b->body.active) return i;
    }
    return DC_BODY_SLOT_NONE;
}
bool fr_gpu_revolute_add(dc_gpu_t *g,const fr_revolute_joint_t *joint,char *err,uint32_t cap) {
    if(!g||!g->runtime_bodies||!joint||!joint->id||!joint->body_a||!joint->body_b||joint->body_a==joint->body_b)
        return error(err,cap,"Invalid revolute joint identities");
    for(unsigned axis=0;axis<2;axis++)
        if(!isfinite(joint->anchor_a[axis])||!isfinite(joint->anchor_b[axis]))
            return error(err,cap,"Invalid revolute anchor");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot reach joint authoring boundary");
    uint32_t a=find_body(g,joint->body_a),b=find_body(g,joint->body_b);
    if(a==DC_BODY_SLOT_NONE||b==DC_BODY_SLOT_NONE) return error(err,cap,"Revolute endpoint is absent");
    if(!dc_gpu_solver_revolute_prepare(g,err,cap)) return false;
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin joint identity admission");
    dc_gpu_runtime_record_body_index(g);
    if(!fr_renderer_submit(g)) return error(err,cap,"Cannot admit joint identities");
    if(!g->revolute) g->revolute=calloc(1,sizeof(*g->revolute));
    if(!g->revolute) return error(err,cap,"Cannot allocate joint context");
    struct fr_revolute_gpu *r=g->revolute;uint32_t index=r->span;
    for(uint32_t i=0;i<r->span;i++) {
        uint32_t *p=(uint32_t *)r->records.mapped+i*JOINT_WORDS;
        if(p[1]&&p[0]==joint->id) return error(err,cap,"Joint identity is already live");
        if(!p[1]&&index==r->span) index=i;
    }
    if(!reserve(g,index+1u,err,cap)) return false;
    uint32_t *p=(uint32_t *)r->records.mapped+index*JOINT_WORDS;
    memset(p,0,JOINT_WORDS*4u);
    p[0]=joint->id;p[1]=1;p[2]=a;p[3]=dc_gpu_body_owner(g,a)->generation;p[4]=joint->body_a;
    p[5]=b;p[6]=dc_gpu_body_owner(g,b)->generation;p[7]=joint->body_b;
    memcpy(p+8,joint->anchor_a,8);memcpy(p+10,joint->anchor_b,8);
    if(index==r->span) r->span++;
    fr_gpu_revolute_bind_work(g,g->runtime_bodies->work.mapped);
    g->descriptor_revision++;
    return true;
}
bool fr_gpu_revolute_remove(dc_gpu_t *g,uint32_t id,char *err,uint32_t cap) {
    if(!g||!g->revolute||!id) return error(err,cap,"Invalid revolute release");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot reach joint release boundary");
    struct fr_revolute_gpu *r=g->revolute;
    for(uint32_t i=0;i<r->span;i++) {
        uint32_t *p=(uint32_t *)r->records.mapped+i*JOINT_WORDS;
        if(p[0]==id&&p[1]) {p[1]=0;return true;}
    }
    return error(err,cap,"Live revolute joint was not found");
}
