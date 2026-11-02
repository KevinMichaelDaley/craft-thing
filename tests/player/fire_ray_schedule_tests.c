#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ferrum/fire/gas.h"
#include "../../src/player/fire_ray_gpu.c"

static int g_pass,g_fail;
static uint32_t g_submits;
static bool g_begin_ok,g_submit_ok;
static VkResult g_queue_result;
#define RUN(fn) do {printf("RUN  %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)) {printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

/* Exercise the production host submission path without a Vulkan device. */
VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue queue) {(void)queue;return g_queue_result;}
VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(VkDevice device,uint32_t count,
    const VkWriteDescriptorSet *writes,uint32_t copies,const VkCopyDescriptorSet *copy) {
    (void)device;(void)count;(void)writes;(void)copies;(void)copy;
}
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer command,VkPipelineBindPoint point,VkPipeline pipeline) {
    (void)command;(void)point;(void)pipeline;
}
VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer command,VkPipelineBindPoint point,
    VkPipelineLayout layout,uint32_t first,uint32_t count,const VkDescriptorSet *sets,
    uint32_t offsets,const uint32_t *offset) {
    (void)command;(void)point;(void)layout;(void)first;(void)count;(void)sets;(void)offsets;(void)offset;
}
VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer command,VkPipelineLayout layout,
    VkShaderStageFlags stages,uint32_t offset,uint32_t size,const void *values) {
    (void)command;(void)layout;(void)stages;(void)offset;(void)size;(void)values;
}
VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(VkCommandBuffer command,uint32_t x,uint32_t y,uint32_t z) {
    (void)command;(void)x;(void)y;(void)z;
}
VkDescriptorBufferInfo dc_gpu_runtime_bodies_binding(const dc_gpu_t *gpu) {
    (void)gpu;return (VkDescriptorBufferInfo){0};
}
bool fr_renderer_begin(dc_gpu_t *gpu) {(void)gpu;return g_begin_ok;}
bool fr_renderer_submit(dc_gpu_t *gpu) {(void)gpu;g_submits++;return g_submit_ok;}
void fr_renderer_barrier(VkCommandBuffer command,VkPipelineStageFlags2 source,VkAccessFlags2 access,
    VkPipelineStageFlags2 destination,VkAccessFlags2 destination_access) {
    (void)command;(void)source;(void)access;(void)destination;(void)destination_access;
}
void fr_gas_gpu_invalidate_cache(dc_gpu_t *gpu) {
    gpu->gas->cache_valid=false;gpu->gas->acoustic.matrix_valid=false;
}

static dc_gpu_t gpu;
static struct fr_gas_gpu gas;
static struct fr_thermal_gpu thermal;
static struct fr_reactive_gpu reactive;
static uint32_t words[DC_FIRE_RAY_HEADER+DC_FIRE_RAY_WORDS];
static fr_fire_ray_gpu_t ray;
static void reset(uint32_t policy) {
    memset(&gpu,0,sizeof(gpu));memset(&gas,0,sizeof(gas));memset(&ray,0,sizeof(ray));
    gpu.gas=&gas;gpu.thermal=&thermal;gpu.reactive=&reactive;
    gas.policy=policy;gas.cache_valid=true;gas.acoustic.matrix_valid=true;
    ray.gpu=&gpu;ray.count=1;ray.duration=.4f;ray.commands.mapped=words;
    ray.bindings[0]=(fr_player_body_binding_t){.identity=1,.body_id=2,.generation=1};g_submits=0;
    g_queue_result=VK_SUCCESS;g_begin_ok=true;g_submit_ok=true;
}
static void test_heat_preserves_advective_bound_but_changes_acoustic_bound(void) {
    fr_gas_model_t model={.gas_constant={287,297,250,461,0},
        .heat_capacity_cv={718,743,800,1400,700},.kinetic_scale=1};
    fr_gas_state_t cold={.species={.23,.77,0,0,0},.momentum={3,-4}};
    double capacity=.23*718+.77*743;
    cold.energy=capacity*293.15+12.5;
    fr_gas_state_t hot=cold;hot.energy+=capacity*(1800-293.15);
    fr_gas_thermo_t a,b;char err[256];
    ASSERT_TRUE(fr_gas_thermodynamics(&model,&cold,1,&a,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_thermodynamics(&model,&hot,1,&b,err,sizeof(err)));
    ASSERT_EQ(a.mass,b.mass);
    ASSERT_EQ(fabs(cold.momentum[0]/a.mass)+fabs(cold.momentum[1]/a.mass),
        fabs(hot.momentum[0]/b.mass)+fabs(hot.momentum[1]/b.mass));
    ASSERT_TRUE(b.sound_speed>2*a.sound_speed);
    ASSERT_TRUE(b.pressure>6*a.pressure);
    PASS();
}
static void test_practical_heat_keeps_transport_and_pressure_matrix_cache(void) {
    reset(FR_GAS_PRESSURE_PRACTICAL);char err[256];
    fr_fire_ray_input_t input={.identity=1,.sequence=1,.facing=1,.emit=true};
    ASSERT_TRUE(fr_fire_ray_gpu_submit(&ray,&input,1,1.f/60,err,sizeof(err)));
    ASSERT_INT_EQ(g_submits,1);
    ASSERT_TRUE(gas.cache_valid&&gas.acoustic.matrix_valid);
    input.emit=false;
    ASSERT_TRUE(fr_fire_ray_gpu_submit(&ray,&input,1,1.f/60,err,sizeof(err)));
    ASSERT_INT_EQ(g_submits,2);
    ASSERT_TRUE(gas.cache_valid&&gas.acoustic.matrix_valid);
    PASS();
}
static void test_acoustic_policies_invalidate_after_heat(void) {
    for(uint32_t policy=FR_GAS_PRESSURE_HLLC;policy<FR_GAS_PRESSURE_PRACTICAL;policy++) {
        reset(policy);char err[256];
        fr_fire_ray_input_t input={.identity=1,.sequence=1,.facing=1,.emit=true};
        ASSERT_TRUE(fr_fire_ray_gpu_submit(&ray,&input,1,1.f/60,err,sizeof(err)));
        ASSERT_INT_EQ(g_submits,1);
        ASSERT_TRUE(!gas.cache_valid&&!gas.acoustic.matrix_valid);
    }
    PASS();
}
static void test_idle_cast_adapter_does_no_work_or_invalidation(void) {
    reset(FR_GAS_PRESSURE_HLLC);char err[256];
    fr_fire_ray_input_t input={.identity=1,.sequence=1,.facing=1};
    ASSERT_TRUE(fr_fire_ray_gpu_submit(&ray,&input,1,1.f/60,err,sizeof(err)));
    ASSERT_INT_EQ(g_submits,0);
    ASSERT_TRUE(gas.cache_valid&&gas.acoustic.matrix_valid);
    PASS();
}
static void test_failed_submission_keeps_cast_retryable(void) {
    for(uint32_t failure=0;failure<3;failure++) {
        reset(FR_GAS_PRESSURE_PRACTICAL);char err[256];
        fr_fire_ray_input_t input={.identity=1,.sequence=1,.facing=1,.emit=true};
        if(failure==0) g_queue_result=VK_ERROR_DEVICE_LOST;
        if(failure==1) g_begin_ok=false;
        if(failure==2) g_submit_ok=false;
        ASSERT_TRUE(!fr_fire_ray_gpu_submit(&ray,&input,1,1.f/60,err,sizeof(err)));
        ASSERT_EQ(ray.sequence[0],0u);
        ASSERT_EQ(ray.remaining[0],0.f);
        g_queue_result=VK_SUCCESS;g_begin_ok=true;g_submit_ok=true;
        ASSERT_TRUE(fr_fire_ray_gpu_submit(&ray,&input,1,1.f/60,err,sizeof(err)));
        ASSERT_EQ(ray.sequence[0],1u);
        ASSERT_EQ(words[DC_FIRE_RAY_HEADER+4],1u);
        ASSERT_TRUE(ray.remaining[0]>0);
        ASSERT_TRUE(fr_fire_ray_gpu_submit(&ray,&input,1,1.f/60,err,sizeof(err)));
        ASSERT_EQ(words[DC_FIRE_RAY_HEADER+4],0u);
    }
    PASS();
}
static void test_failed_active_step_preserves_remaining_duration(void) {
    reset(FR_GAS_PRESSURE_PRACTICAL);char err[256];
    fr_fire_ray_input_t input={.identity=1,.sequence=1,.facing=1,.emit=true};
    ASSERT_TRUE(fr_fire_ray_gpu_submit(&ray,&input,1,1.f/60,err,sizeof(err)));
    float before=ray.remaining[0];input.emit=false;g_begin_ok=false;
    ASSERT_TRUE(!fr_fire_ray_gpu_submit(&ray,&input,1,1.f/60,err,sizeof(err)));
    ASSERT_EQ(ray.remaining[0],before);
    g_begin_ok=true;
    ASSERT_TRUE(fr_fire_ray_gpu_submit(&ray,&input,1,1.f/60,err,sizeof(err)));
    ASSERT_TRUE(fabsf(ray.remaining[0]-(before-1.f/60))<.000001f);
    PASS();
}
int main(void) {
    RUN(test_heat_preserves_advective_bound_but_changes_acoustic_bound);
    RUN(test_practical_heat_keeps_transport_and_pressure_matrix_cache);
    RUN(test_acoustic_policies_invalidate_after_heat);
    RUN(test_idle_cast_adapter_does_no_work_or_invalidation);
    RUN(test_failed_submission_keeps_cast_retryable);
    RUN(test_failed_active_step_preserves_remaining_duration);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
