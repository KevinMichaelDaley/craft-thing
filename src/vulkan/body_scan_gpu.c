#include <stdio.h>
#include <stdlib.h>
#include "body_scan_gpu.h"
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static uint32_t groups(uint32_t count) {return (count-1u)/DC_BODY_SCAN_LANES+1u;}
bool dc_gpu_body_scan_plan_create(uint32_t count,uint32_t max_groups_x,uint32_t max_groups_y,
    dc_gpu_body_scan_plan_t *out,char *err,uint32_t cap) {
    if(!count||!max_groups_x||!max_groups_y||!out||out->levels)
        return error(err,cap,"Invalid body scan resource plan");
    uint32_t level_count=1;
    for(uint32_t length=count;groups(length)>1u;length=groups(length)) level_count++;
    dc_gpu_body_scan_plan_t pending={.level_count=level_count};
    pending.levels=calloc(level_count,sizeof(*pending.levels));
    if(!pending.levels) return error(err,cap,"Cannot allocate body scan dispatch metadata");
    uint32_t length=count;
    for(uint32_t level=0;level<level_count;level++) {
        uint32_t workgroups=groups(length),x=workgroups<max_groups_x?workgroups:max_groups_x;
        uint32_t y=(workgroups-1u)/x+1u;
        if(y>max_groups_y) {
            dc_gpu_body_scan_plan_destroy(&pending);
            return error(err,cap,"Body scan exceeds device dispatch dimensions");
        }
        pending.levels[level]=(dc_gpu_body_scan_level_t){.count=length,.groups=workgroups,
            .groups_x=x,.groups_y=y,.offset=pending.bytes};
        pending.bytes+=(VkDeviceSize)length*2u*sizeof(uint32_t);length=workgroups;
    }
    pending.total_offset=pending.bytes;pending.bytes+=2u*sizeof(uint32_t);
    *out=pending;return true;
}
void dc_gpu_body_scan_plan_destroy(dc_gpu_body_scan_plan_t *plan) {
    if(plan) {free(plan->levels);*plan=(dc_gpu_body_scan_plan_t){0};}
}
