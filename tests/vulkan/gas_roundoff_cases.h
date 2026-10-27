#ifndef GAS_ROUNDOFF_CASES_H
#define GAS_ROUNDOFF_CASES_H
#include <float.h>
#include "dungeoncraft/gas_layout.h"
#include "../../src/vulkan/body_address_gpu.h"
#include "../../src/vulkan/renderer_internal.h"
static void test_native_massless_underflow_energy_reconciles_without_erasing_tiny_colored_gas(void) {
    char err[256]={0};dc_gpu_t *g=NULL;ASSERT_TRUE(context(&g,err,sizeof(err)));
    dc_gpu_address_buffer_t input={0},output={0},model={0},stats={0};fr_renderer_compute_t compute={0};
    ASSERT_TRUE(dc_gpu_address_buffer_create(g,9u*64u,true,&input,err,sizeof(err))&&
        dc_gpu_address_buffer_create(g,9u*64u,true,&output,err,sizeof(err))&&
        dc_gpu_address_buffer_create(g,FR_GAS_MODEL_WORDS*4u,true,&model,err,sizeof(err))&&
        dc_gpu_address_buffer_create(g,64u,true,&stats,err,sizeof(err)));
    float (*packets)[16]=input.mapped;for(uint32_t i=0;i<9u;i++) packets[i][12]=packets[i][13]=1.f;
    packets[0][5]=1.14107925e-37f;packets[1][5]=-1.14107925e-37f;
    fr_gas_model_t m=reference_model();m.kinetic_scale=1./512.;
    fr_gas_state_t trace=reference_air(m,.0012,300,0);
    for(uint32_t i=0;i<5u;i++) packets[2][i]=(float)trace.species[i];
    packets[2][2]=1e-39f;packets[2][5]=(float)trace.energy;packets[2][8]=9e-40f;
    packets[3][5]=1e-3f;
    fr_gas_state_t air=reference_air(m,.0012,300,2);
    for(uint32_t i=0;i<5u;i++) packets[4][i]=(float)air.species[i];
    packets[4][5]=(float)air.energy;packets[4][6]=(float)air.momentum[0];
    float envelope=.0012f*300.f*(float)fmax(m.heat_capacity_cv[0],m.heat_capacity_cv[1])*(8.f*FLT_EPSILON);
    packets[5][5]=2.f*envelope;packets[6][5]=.5f*envelope;packets[7][5]=1e-37f;packets[7][11]=-1e-37f;
    /* Real hot-fire cut-cell packet: neither mass nor energy may be erased. */
    packets[8][0]=3.29207104e-38f;packets[8][1]=1.37108437e-38f;
    packets[8][5]=5.02622442e-38f;packets[8][12]=packets[8][13]=.000244140625f;
    float *parameters=model.mapped;
    for(uint32_t i=0;i<5u;i++) {parameters[i]=(float)m.gas_constant[i];parameters[5u+i]=(float)m.heat_capacity_cv[i];}
    parameters[10]=(float)m.kinetic_scale;parameters[11]=.0012f;parameters[12]=300.f;parameters[16]=3.f;
    VkDescriptorBufferInfo buffers[12];for(uint32_t i=0;i<12u;i++) buffers[i]=(VkDescriptorBufferInfo){input.buffer,0,input.bytes};
    buffers[1]=(VkDescriptorBufferInfo){output.buffer,0,output.bytes};buffers[5]=(VkDescriptorBufferInfo){model.buffer,0,model.bytes};buffers[6]=(VkDescriptorBufferInfo){stats.buffer,0,stats.bytes};
    ASSERT_TRUE(fr_renderer_compute_init(g,&compute,buffers,12,16,"build/shaders/gas_roundoff_test.comp.spv",err,sizeof(err))&&fr_renderer_begin(g));
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    uint32_t push[4]={9,1,0,FR_GAS_PRACTICAL_BIT};vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute.pipeline_layout,0,1,&compute.descriptor,0,NULL);
    vkCmdPushConstants(g->command,compute.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);vkCmdDispatch(g->command,1,1,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    ASSERT_TRUE(fr_renderer_submit(g));uint32_t mask=((uint32_t *)stats.mapped)[0];printf("Vacuum roundoff valid mask%u expected471\n",mask);ASSERT_EQ(mask,471u);
    ASSERT_TRUE((((uint32_t *)stats.mapped)[FR_GAS_REJECT_STAGE_WORD]&FR_GAS_RECONCILED_ROUNDOFF)!=0u);
    float (*actual)[16]=output.mapped;ASSERT_EQ(memcmp(actual[8],packets[8],64),0);ASSERT_EQ(memcmp(actual[2],packets[2],64),0);ASSERT_EQ(memcmp(actual[4],packets[4],64),0);ASSERT_EQ(memcmp(actual[7],packets[7],64),0);
    ASSERT_EQ(actual[3][5],packets[3][5]);ASSERT_EQ(actual[5][5],packets[5][5]);
    ASSERT_TRUE(present(g,err,sizeof(err)));fr_renderer_compute_destroy(g,&compute);
    dc_gpu_address_buffer_destroy(g,&input);dc_gpu_address_buffer_destroy(g,&output);dc_gpu_address_buffer_destroy(g,&model);dc_gpu_address_buffer_destroy(g,&stats);dc_gpu_destroy(g);PASS();
}
#endif
