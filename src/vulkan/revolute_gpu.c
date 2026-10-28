#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/physics/revolute_gpu.h"
#include "revolute_gpu_internal.h"
#include "runtime_body_internal.h"

enum { JOINT_WORDS=24, JOINT_WORK_HEADER=96 };
_Static_assert(sizeof(fr_revolute_joint_t)==60u&&offsetof(fr_revolute_joint_t,lower_angle)==32u,
               "Revolute settings layout changed");
struct fr_revolute_gpu {dc_gpu_address_buffer_t records;uint32_t span,capacity;};
static uint32_t hash_capacity(uint32_t capacity) {
    uint32_t size=1;while(size<2u*(uint64_t)capacity) size*=2u;return size;
}
static uint32_t pair_hash(uint32_t a,uint32_t b) {
    uint32_t x=a*0x9e3779b9u^b*0x85ebca6bu;x^=x>>16u;return x;
}
static void rebuild_hash(struct fr_revolute_gpu *r) {
    uint32_t *words=r->records.mapped,*table=words+r->capacity*JOINT_WORDS;
    uint32_t count=hash_capacity(r->capacity);memset(table,0,(size_t)count*16u);
    for(uint32_t i=0;i<r->span;i++) {
        uint32_t *p=words+i*JOINT_WORDS;if(!p[1]||(p[12]&FR_REVOLUTE_COLLIDE_CONNECTED)) continue;
        uint32_t a=p[2]<p[5]?p[2]:p[5],b=p[2]>p[5]?p[2]:p[5];
        uint32_t hash=pair_hash(a,b)&(count-1u);
        while(table[hash*4u+2u]) hash=(hash+1u)&(count-1u);
        table[hash*4u]=a;table[hash*4u+1u]=b;table[hash*4u+2u]=i+1u;
    }
}
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
    h[JOINT_WORK_HEADER+3u]=r?r->capacity*JOINT_WORDS:0;
    h[JOINT_WORK_HEADER+4u]=r?hash_capacity(r->capacity):0;
}
void fr_gpu_revolute_destroy(dc_gpu_t *g) {
    if(!g||!g->revolute) return;
    fr_gpu_revolute_discard(g,g->revolute);g->revolute=NULL;
}
void fr_gpu_revolute_discard(dc_gpu_t *g,struct fr_revolute_gpu *r) {
    if(r) {dc_gpu_address_buffer_destroy(g,&r->records);free(r);}
}
void fr_gpu_revolute_publish(dc_gpu_t *g,struct fr_revolute_gpu *r) {
    fr_gpu_revolute_destroy(g);g->revolute=r;
    fr_gpu_revolute_bind_work(g,g->runtime_bodies->work.mapped);g->descriptor_revision++;
}
static bool reserve(dc_gpu_t *g,uint32_t required,char *err,uint32_t cap) {
    struct fr_revolute_gpu *r=g->revolute;
    if(required<=r->capacity) return true;
    uint64_t next=r->capacity?r->capacity:8u;
    while(next<required) next*=2u;
    if(next>UINT32_MAX/(JOINT_WORDS+16u)) return error(err,cap,"Joint resource size overflow");
    dc_gpu_address_buffer_t pending={0};
    uint64_t words=next*JOINT_WORDS+(uint64_t)hash_capacity((uint32_t)next)*4u;
    if(!dc_gpu_address_buffer_create(g,words*4u,true,&pending,err,cap)) return false;
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
        if(b->body.id==id&&(b->body.active||b->reserved[0]==DC_GPU_STRUCTURAL_TAG)) return i;
    }
    return DC_BODY_SLOT_NONE;
}
static bool joint_valid(const fr_revolute_joint_t *joint,char *err,uint32_t cap) {
    if(!joint||!joint->id||!joint->body_a||!joint->body_b||joint->body_a==joint->body_b)
        return error(err,cap,"Invalid revolute joint identities");
    for(unsigned axis=0;axis<2;axis++)
        if(!isfinite(joint->anchor_a[axis])||!isfinite(joint->anchor_b[axis])||
           fabsf(joint->anchor_a[axis])>1000000.f||fabsf(joint->anchor_b[axis])>1000000.f)
            return error(err,cap,"Invalid revolute anchor");
    const float settings[]={joint->lower_angle,joint->upper_angle,joint->spring_stiffness,
        joint->spring_rest_angle,joint->friction_torque,joint->max_force,joint->max_torque};
    for(unsigned i=0;i<sizeof(settings)/sizeof(*settings);i++)
        if(!isfinite(settings[i])||fabsf(settings[i])>1000000.f)
            return error(err,cap,"Invalid revolute mechanical settings");
    if((joint->flags&~7u)||joint->spring_stiffness<0||joint->friction_torque<0||
       joint->max_force<0||joint->max_torque<0||
       ((joint->flags&FR_REVOLUTE_LIMITS)&&joint->lower_angle>joint->upper_angle))
        return error(err,cap,"Invalid revolute limits or capacity");
    return true;
}
bool fr_gpu_revolute_add(dc_gpu_t *g,const fr_revolute_joint_t *joint,char *err,uint32_t cap) {
    if(!g||!g->runtime_bodies||!joint_valid(joint,err,cap)) return false;
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
    p[12]=joint->flags;memcpy(p+13,&joint->lower_angle,7u*sizeof(float));
    if(index==r->span) r->span++;
    rebuild_hash(r);
    fr_gpu_revolute_bind_work(g,g->runtime_bodies->work.mapped);
    g->descriptor_revision++;
    return true;
}
static int compare_ids(const void *a,const void *b) {
    uint32_t x=*(const uint32_t *)a,y=*(const uint32_t *)b;return (x>y)-(x<y);
}
static bool record_live(dc_gpu_t *g,const uint32_t *p) {
    if(!p[1]) return false;
    for(unsigned endpoint=0;endpoint<2;endpoint++) {
        unsigned at=endpoint?5u:2u;uint32_t slot=p[at];
        if(slot>=dc_gpu_body_capacity(g)) return false;
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,slot);
        if(owner->state!=DC_BODY_OWNER_LIVE||owner->generation!=p[at+1u]||owner->runtime_id!=p[at+2u]||
           (!dc_gpu_body_record(g,slot)->body.active&&dc_gpu_body_record(g,slot)->reserved[0]!=DC_GPU_STRUCTURAL_TAG)) return false;
    }
    return true;
}
bool fr_gpu_revolute_collect(dc_gpu_t *g,uint8_t **bytes,size_t *length,char *err,uint32_t cap) {
    *bytes=NULL;*length=0;
    struct fr_revolute_gpu *r=g->revolute;if(!r) return true;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot collect pivot checkpoint");
    uint32_t count=0,*source=r->records.mapped;
    for(uint32_t i=0;i<r->span;i++) if(record_live(g,source+i*JOINT_WORDS)) count++;
    if(!count) return true;
    size_t size=16u+(size_t)count*JOINT_WORDS*4u;
    uint32_t *out=calloc(1,size);if(!out) return error(err,cap,"Cannot allocate pivot checkpoint");
    out[0]=0x314a4344u;out[1]=1;out[2]=JOINT_WORDS;out[3]=count;
    count=0;
    for(uint32_t i=0;i<r->span;i++) if(record_live(g,source+i*JOINT_WORDS)) {
        uint32_t *p=out+4u+count++*JOINT_WORDS;memcpy(p,source+i*JOINT_WORDS,JOINT_WORDS*4u);
        memset(p+20u,0,4u*sizeof(uint32_t));
    }
    *bytes=(uint8_t *)out;*length=size;return true;
}
bool fr_gpu_revolute_prepare(dc_gpu_t *g,const uint8_t *bytes,size_t length,
    const dc_structural_store_payload_t *payload,struct fr_revolute_gpu **prepared,char *err,uint32_t cap) {
    *prepared=NULL;if(!length) return true;
    if(length<16u||length%4u) return error(err,cap,"Invalid pivot checkpoint envelope");
    const uint32_t *input=(const uint32_t *)bytes;
    uint32_t count=input[3];
    if(input[0]!=0x314a4344u||input[1]!=1u||input[2]!=JOINT_WORDS||!count||
       (uint64_t)count*JOINT_WORDS*4u+16u!=length)
        return error(err,cap,"Invalid pivot checkpoint layout");
    uint32_t *ids=calloc(count,sizeof(*ids));if(!ids) return error(err,cap,"Cannot validate pivot identities");
    bool valid=true;
    for(uint32_t i=0;i<count&&valid;i++) {
        const uint32_t *p=input+4u+i*JOINT_WORDS;
        fr_revolute_joint_t joint={.id=p[0],.body_a=p[4],.body_b=p[7],.flags=p[12]};
        memcpy(joint.anchor_a,p+8u,8u);memcpy(joint.anchor_b,p+10u,8u);
        memcpy(&joint.lower_angle,p+13u,7u*sizeof(float));ids[i]=joint.id;
        valid=p[1]==1u&&joint_valid(&joint,err,cap)&&p[2]<payload->body_count&&p[5]<payload->body_count;
        for(uint32_t k=20;k<24;k++) valid=valid&&p[k]==0u;
        for(uint32_t endpoint=0;endpoint<2&&valid;endpoint++) {
            uint32_t at=endpoint?5u:2u,slot=p[at];
            dc_body_storage_owner_t owner=payload->states[slot].owner;
            valid=owner.state==DC_BODY_OWNER_LIVE&&owner.generation==p[at+1u]&&
                owner.runtime_id==p[at+2u]&&(payload->bodies[slot].body.active||payload->bodies[slot].reserved[0]==DC_GPU_STRUCTURAL_TAG)&&
                payload->bodies[slot].body.id==p[at+2u];
        }
    }
    if(valid) {
        qsort(ids,count,sizeof(*ids),compare_ids);
        for(uint32_t i=1;i<count;i++) if(ids[i]==ids[i-1u]) valid=false;
    }
    free(ids);if(!valid) return error(err,cap,"Invalid pivot checkpoint identity or settings");
    if(!dc_gpu_solver_revolute_prepare(g,err,cap)) return false;
    struct fr_revolute_gpu *r=calloc(1,sizeof(*r));
    if(!r) return error(err,cap,"Cannot prepare pivot checkpoint");
    if(count>UINT32_MAX/(JOINT_WORDS+16u)) {free(r);return error(err,cap,"Pivot checkpoint resource overflow");}
    VkDeviceSize words=(VkDeviceSize)count*JOINT_WORDS+(VkDeviceSize)hash_capacity(count)*4u;
    if(!dc_gpu_address_buffer_create(g,words*4u,true,&r->records,err,cap)) {free(r);return false;}
    memcpy(r->records.mapped,input+4u,(size_t)count*JOINT_WORDS*4u);r->span=r->capacity=count;
    rebuild_hash(r);
    *prepared=r;return true;
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
