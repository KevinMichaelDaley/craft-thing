#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpu_internal.h"
#include "revolute_gpu_internal.h"

typedef struct {
    int64_t origin_x,origin_y;
    uint64_t revision;
    uint64_t width,height,count,capacity,pairs,contacts;
    VkBuffer body,activity;
    VkDescriptorSet descriptor;
    VkPipelineLayout layout;
    uint64_t structural,structural_graph,runtime,fused_grain,apply_overlap,mpm_fused_grid;
    uint64_t fluid_scale,fluid_timed,fluid_occupancy,pressure_work;
    uint64_t joints;
} solver_command_key_t;
enum { RIGID_PROGRAM, MPM_PROGRAM, FLUID_PROGRAM, COMMAND_PROGRAM_COUNT=FLUID_PROGRAM+10 };
typedef struct {
    VkCommandBuffer command;
    solver_command_key_t key;
    uint64_t recordings,executions;
    bool valid,disabled;
} solver_command_program_t;
struct dc_solver_commands {solver_command_program_t programs[COMMAND_PROGRAM_COUNT];};

static solver_command_key_t command_key(const dc_gpu_t *g,unsigned program) {
    solver_command_key_t key;memset(&key,0,sizeof(key));
    key.origin_x=g->body_origin.x;key.origin_y=g->body_origin.y;
    key.revision=g->descriptor_revision;
    key.width=g->width;key.height=g->height;
    key.count=dc_gpu_runtime_body_count(g);key.capacity=dc_gpu_body_capacity(g);
    key.pairs=g->runtime_pair_capacity;key.contacts=g->runtime_contact_capacity;
    key.body=g->body_buffer;key.activity=g->mpm_activity_buffer;
    key.descriptor=g->descriptor;key.layout=g->pipeline_layout;
    key.structural=g->structural_enabled;key.runtime=g->runtime_body_consumers;
    key.structural_graph=g->structural_graph_possible;
    key.fused_grain=g->solver_fused_grain;
    key.apply_overlap=g->solver_apply_overlap;
    key.joints=fr_gpu_revolute_span(g);
    key.mpm_fused_grid=g->mpm_fused_grid;
    if(program>=FLUID_PROGRAM) {
        key.fluid_scale=dc_gpu_float_bits(g->fluid_step_scale);
        key.fluid_timed=g->timed_fluid;key.fluid_occupancy=g->rigid_occupancy_present;
        key.pressure_work=g->pressure_work_enabled;
    }
    return key;
}
bool dc_gpu_solver_commands_init(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!g||!g->device||!g->command_pool||g->solver_commands) {
        if(err&&cap) snprintf(err,cap,"Invalid solver command cache destination");
        return false;
    }
    struct dc_solver_commands *cache=calloc(1,sizeof(*cache));
    if(!cache) {if(err&&cap) snprintf(err,cap,"Cannot allocate solver command cache");return false;}
    VkCommandBufferAllocateInfo allocation={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool=g->command_pool,.level=VK_COMMAND_BUFFER_LEVEL_SECONDARY,.commandBufferCount=1};
    g->solver_commands=cache;
    for(unsigned program=0;program<COMMAND_PROGRAM_COUNT;program++) {
        if(vkAllocateCommandBuffers(g->device,&allocation,&cache->programs[program].command)!=VK_SUCCESS) {
            dc_gpu_solver_commands_destroy(g);
            if(err&&cap) snprintf(err,cap,"Cannot allocate reusable solver commands");
            return false;
        }
    }
    const char *control=getenv("DC_SOLVER_COMMAND_CACHE");
    if(control&&strcmp(control,"0")==0) g->solver_cache_disabled=true;
    const char *mpm_control=getenv("DC_MPM_COMMAND_CACHE");
    if(mpm_control&&strcmp(mpm_control,"0")==0) cache->programs[MPM_PROGRAM].disabled=true;
    g->solver_commands=cache;return true;
}
void dc_gpu_solver_commands_destroy(dc_gpu_t *g) {
    if(!g||!g->solver_commands) return;
    struct dc_solver_commands *cache=g->solver_commands;
    for(unsigned program=0;program<COMMAND_PROGRAM_COUNT;program++)
        if(cache->programs[program].command&&g->command_pool)
            vkFreeCommandBuffers(g->device,g->command_pool,1,&cache->programs[program].command);
    if(getenv("DC_COMMAND_CACHE_STATS")) {
        for(unsigned program=0;program<COMMAND_PROGRAM_COUNT;program++)
            fprintf(stderr,"command cache program%u: recordings%llu executions%llu\n",program,
                (unsigned long long)cache->programs[program].recordings,
                (unsigned long long)cache->programs[program].executions);
    }
    free(cache);g->solver_commands=NULL;
}
bool dc_gpu_solver_commands_stats(const dc_gpu_t *g,uint64_t *recordings,uint64_t *executions) {
    if(!g||!g->solver_commands||!recordings||!executions) return false;
    *recordings=g->solver_commands->programs[RIGID_PROGRAM].recordings;
    *executions=g->solver_commands->programs[RIGID_PROGRAM].executions;return true;
}
static void record_program(dc_gpu_t *g,unsigned program) {
    if(program==RIGID_PROGRAM) dc_gpu_record_solver_uncached(g);
    else if(program==MPM_PROGRAM) dc_gpu_record_mpm_uncached(g);
    else dc_gpu_record_fluid_phase_uncached(g,(program-FLUID_PROGRAM)/2u);
}
static bool execute_cached(dc_gpu_t *g,unsigned program) {
    if(!g->solver_commands||g->solver_cache_disabled||g->component_profile) return false;
    solver_command_program_t *cache=&g->solver_commands->programs[program];
    if(cache->disabled) return false;
    solver_command_key_t key=command_key(g,program);
    if(!cache->valid||memcmp(&key,&cache->key,sizeof(key))!=0) {
        cache->valid=false;
        VkCommandBufferInheritanceInfo inheritance={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
        VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags=VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,.pInheritanceInfo=&inheritance};
        if(vkResetCommandBuffer(cache->command,0)!=VK_SUCCESS||
           vkBeginCommandBuffer(cache->command,&begin)!=VK_SUCCESS) {
            g->solver_cache_disabled=true;return false;
        }
        VkCommandBuffer primary=g->command;g->command=cache->command;
        vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,
            0,1,&g->descriptor,0,NULL);
        record_program(g,program);
        VkResult result=vkEndCommandBuffer(g->command);g->command=primary;
        if(result!=VK_SUCCESS) {g->solver_cache_disabled=true;return false;}
        cache->key=key;cache->valid=true;cache->recordings++;
    }
    vkCmdExecuteCommands(g->command,1,&cache->command);
    /* Compute state is not inherited across secondary command execution. */
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,
        0,1,&g->descriptor,0,NULL);
    cache->executions++;return true;
}
void dc_gpu_record_solver(dc_gpu_t *g) {
    if(!execute_cached(g,RIGID_PROGRAM)) dc_gpu_record_solver_uncached(g);
}
void dc_gpu_record_mpm(dc_gpu_t *g) {
    if(!execute_cached(g,MPM_PROGRAM)) dc_gpu_record_mpm_uncached(g);
}

void dc_gpu_record_fluid_phase(dc_gpu_t *g,uint32_t phase) {
    dc_gpu_fluid_uploads_invalidate(g);
    if(phase>=5u||!execute_cached(g,FLUID_PROGRAM+phase*2u+(g->fluid_tick&1u))) {
        dc_gpu_record_fluid_phase_uncached(g,phase);
        return;
    }
    if(phase==3u) g->fluid_snapshot_valid=true;
}
