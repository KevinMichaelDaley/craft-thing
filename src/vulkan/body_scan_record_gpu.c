#include "body_scan_gpu.h"
static void barrier(VkCommandBuffer command,bool first,bool indirect) {
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|(first?VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT:0),
        .srcAccessMask=VK_ACCESS_2_SHADER_WRITE_BIT|(first?VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT:0),
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask=VK_ACCESS_2_SHADER_READ_BIT|VK_ACCESS_2_SHADER_WRITE_BIT};
    if(indirect) {
        memory.dstStageMask|=VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
        memory.dstAccessMask|=VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    }
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(command,&dependency);
}
static void dispatch(VkCommandBuffer command,VkPipelineLayout layout,
    const dc_gpu_body_scan_level_t *level,VkDeviceAddress data,VkDeviceAddress parent,
    VkBuffer commands,VkDeviceSize *offset) {
    uint32_t constants[]={(uint32_t)data,(uint32_t)(data>>32),(uint32_t)parent,(uint32_t)(parent>>32),level->count,0};
    vkCmdPushConstants(command,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(constants),constants);
    if(commands) {
        vkCmdDispatchIndirect(command,commands,*offset);
        *offset+=sizeof(VkDispatchIndirectCommand);
    } else vkCmdDispatch(command,level->groups_x,level->groups_y,1);
    barrier(command,false,commands!=VK_NULL_HANDLE);
}
static bool record(VkCommandBuffer command,const dc_gpu_body_scan_plan_t *plan,
    VkDeviceAddress address,VkDeviceSize bytes,VkPipelineLayout layout,VkPipeline scan,VkPipeline add,
    VkBuffer commands,VkDeviceSize offset,VkDeviceSize commands_bytes) {
    if(!command||!plan||!plan->levels||!plan->level_count||!address||(address&7u)||
       !layout||!scan||!add||bytes<plan->bytes||address>UINT64_MAX-plan->bytes) return false;
    if(commands) {
        VkDeviceSize passes=(VkDeviceSize)plan->level_count*2u-1u;
        VkDeviceSize required=passes*sizeof(VkDispatchIndirectCommand);
        if((offset&3u)||offset>commands_bytes||required>commands_bytes-offset) return false;
    }
    barrier(command,true,commands!=VK_NULL_HANDLE);
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,scan);
    for(uint32_t i=0;i<plan->level_count;i++) {
        VkDeviceSize next=i+1u<plan->level_count?plan->levels[i+1u].offset:plan->total_offset;
        dispatch(command,layout,&plan->levels[i],address+plan->levels[i].offset,address+next,commands,&offset);
    }
    if(plan->level_count>1u) {
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,add);
        for(uint32_t i=plan->level_count-1u;i>0u;i--)
            dispatch(command,layout,&plan->levels[i-1u],address+plan->levels[i-1u].offset,address+plan->levels[i].offset,commands,&offset);
    }
    return true;
}
bool dc_gpu_body_scan_record(VkCommandBuffer command,const dc_gpu_body_scan_plan_t *plan,
    VkDeviceAddress address,VkDeviceSize bytes,VkPipelineLayout layout,VkPipeline scan,VkPipeline add) {
    return record(command,plan,address,bytes,layout,scan,add,VK_NULL_HANDLE,0,0);
}
bool dc_gpu_body_scan_record_indirect(VkCommandBuffer command,const dc_gpu_body_scan_plan_t *plan,
    VkDeviceAddress address,VkDeviceSize bytes,VkPipelineLayout layout,VkPipeline scan,VkPipeline add,
    VkBuffer commands,VkDeviceSize commands_offset,VkDeviceSize commands_bytes) {
    if(!commands) return false;
    return record(command,plan,address,bytes,layout,scan,add,commands,commands_offset,commands_bytes);
}
