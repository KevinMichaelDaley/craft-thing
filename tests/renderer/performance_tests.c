#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

#include "../../src/vulkan/gpu_internal.h"
#include "concave_scene.h"

static int g_pass, g_fail;
static VkQueryPool g_queries;
static unsigned g_dispatch;
static bool g_recording, g_subsystem;
static bool g_lighting;
static unsigned g_light_passes;
static bool g_profile_passes;
#define RUN(fn)                                                                \
  do {                                                                         \
    printf("RUN %s\n", #fn);                                                   \
    fn();                                                                      \
  } while (0)
#define ASSERT_TRUE(e)                                                         \
  do {                                                                         \
    if (!(e)) {                                                                \
      printf("FAIL %d: %s\n", __LINE__, #e);                                   \
      g_fail++;                                                                \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++
void __real_fr_renderer_record(dc_gpu_t *g);
void __wrap_fr_renderer_record(dc_gpu_t *g) {
  vkCmdResetQueryPool(g->command, g_queries, 0, 64);
  g_dispatch = 0;
  g_recording = true;
  vkCmdWriteTimestamp2(g->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 0);
  __real_fr_renderer_record(g);
  vkCmdWriteTimestamp2(g->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 1);
  g_recording = false;
}
void __real_vkCmdDispatch(VkCommandBuffer command, uint32_t x, uint32_t y,
                          uint32_t z);
void __wrap_vkCmdDispatch(VkCommandBuffer command, uint32_t x, uint32_t y,
                          uint32_t z) {
  unsigned query = 99;
  if (g_recording && !g_subsystem) {
    if (g_dispatch == 2)
      query = 2;
    else if (g_dispatch == 3)
      query = 4;
    else if (g_dispatch == 5)
      query = 10;
    g_dispatch++;
  }
  if (query != 99)
    vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         g_queries, query);
  __real_vkCmdDispatch(command, x, y, z);
  if (query != 99)
    vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         g_queries, query + 1);
}
void __real_fr_occlusion_record(fr_occlusion_gpu_t *, VkBuffer, VkDeviceSize);
void __wrap_fr_occlusion_record(fr_occlusion_gpu_t *c, VkBuffer b,
                                VkDeviceSize offset) {
  extern VkCommandBuffer g_command;
  g_subsystem = true;
  vkCmdWriteTimestamp2(g_command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 6);
  __real_fr_occlusion_record(c, b, offset);
  vkCmdWriteTimestamp2(g_command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 7);
  g_subsystem = false;
}
void __real_fr_lighting_record(fr_lighting_gpu_t *, VkBuffer, VkDeviceSize);
void __wrap_fr_lighting_record(fr_lighting_gpu_t *l, VkBuffer b,
                               VkDeviceSize offset) {
  extern VkCommandBuffer g_command;
  g_subsystem = true;
  vkCmdWriteTimestamp2(g_command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 8);
  g_lighting = g_profile_passes;
  g_light_passes = 0;
  __real_fr_lighting_record(l, b, offset);
  g_lighting = false;
  vkCmdWriteTimestamp2(g_command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       g_queries, 9);
  g_subsystem = false;
}
VkCommandBuffer g_command;
void __real_vkCmdDispatchIndirect(VkCommandBuffer, VkBuffer, VkDeviceSize);
void __wrap_vkCmdDispatchIndirect(VkCommandBuffer command, VkBuffer buffer,
                                 VkDeviceSize offset) {
  unsigned query = 12u + 2u * g_light_passes;
  if (g_lighting && query + 1u < 64u)
    vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         g_queries, query);
  __real_vkCmdDispatchIndirect(command, buffer, offset);
  if (g_lighting && query + 1u < 64u) {
    vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         g_queries, query + 1u);
    g_light_passes++;
  }
}
static void test_full_frame(unsigned width, unsigned height) {
  dc_gpu_t *g;
  char err[256] = {0};
  ASSERT_TRUE(dc_gpu_create(&g, width, height, "build/shaders/pattern.comp.spv", err,
                            sizeof(err)));
  VkPhysicalDeviceProperties device;
  vkGetPhysicalDeviceProperties(g->physical, &device);
  printf("Vulkan device: %s; fluid pressure sweeps %u, chunk slots %u\n",
         device.deviceName, DC_PRESSURE_SWEEPS, DC_GPU_CHUNK_SLOTS);
  ASSERT_TRUE(width == RENDERER_WIDTH
                  ? renderer_concave_scene(g, err, sizeof(err))
                  : renderer_concave_scene_sized(g, width, height, err,
                                                 sizeof(err)));
  ASSERT_TRUE(renderer_four_lights(g, 4));
  g_command = g->command;
  VkQueryPoolCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                              .queryType = VK_QUERY_TYPE_TIMESTAMP,
                              .queryCount = 64};
  ASSERT_EQ(vkCreateQueryPool(g->device, &qi, NULL, &g_queries), VK_SUCCESS);
  ASSERT_TRUE(
      dc_gpu_set_body_motion(g, 0x80000001u, 0, .005f, 0, err, sizeof(err)));
  ASSERT_TRUE(
      dc_gpu_set_body_motion(g, 0x80000002u, 0, -.003f, 0, err, sizeof(err)));
  double times[6] = {0}, wall = 0, worst = 0;
  double passes[26] = {0};
  unsigned samples = 24;
  for (unsigned i = 0; i < samples; i++) {
    struct timespec start, stop;
    clock_gettime(CLOCK_MONOTONIC, &start);
    ASSERT_TRUE(dc_gpu_tick_step(g, err, sizeof(err)));
    ASSERT_TRUE(fr_renderer_render(g, err, sizeof(err)));
    clock_gettime(CLOCK_MONOTONIC, &stop);
    double frame = (stop.tv_sec - start.tv_sec) * 1000. +
                   (stop.tv_nsec - start.tv_nsec) / 1e6;
    wall += frame;
    if (frame > worst)
      worst = frame;
    uint64_t q[64];
    ASSERT_EQ(vkGetQueryPoolResults(
                  g->device, g_queries, 0, 12u + 2u * g_light_passes, sizeof(q), q, 8,
                  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
              VK_SUCCESS);
    uint64_t mask = g->timestamp_bits == 64
                        ? UINT64_MAX
                        : ((UINT64_C(1) << g->timestamp_bits) - 1);
    if (getenv("DC_RENDERER_FRAME_PROFILE"))
      printf("frame,%ux%u,%u,wall%.3f,renderer%.3f,light%.3f\n", width,
             height, i, frame, ((q[1] - q[0]) & mask) *
             g->timestamp_period / 1e6, ((q[9] - q[8]) & mask) *
             g->timestamp_period / 1e6);
    for (unsigned stage = 0; stage < 6; stage++)
      times[stage] += ((q[2 * stage + 1] - q[2 * stage]) & mask) *
                      g->timestamp_period / 1e6;
    for (unsigned pass = 0; pass < g_light_passes; pass++)
      passes[pass] += ((q[13u + 2u * pass] - q[12u + 2u * pass]) & mask) *
                     g->timestamp_period / 1e6;
  }
  printf("Full frame %ux%u mean %.3f ms, worst %.3f ms "
         "(simulation + render + submission/wait, %u samples)\n",
         width, height, wall / samples, worst, samples);
  printf("Renderer mean GPU ms: total %.3f, texture %.3f, optical %.3f, "
         "hierarchy %.3f, light %.3f, compose %.3f\n",
         times[0] / samples, times[1] / samples, times[2] / samples,
         times[3] / samples, times[4] / samples, times[5] / samples);
  for (unsigned pass = 0; pass < g_light_passes; pass++)
    printf("Light pass %u: %.3f ms\n", pass, passes[pass] / samples);
  vkDestroyQueryPool(g->device, g_queries, NULL);
  g_queries = VK_NULL_HANDLE;
  dc_gpu_destroy(g);
  ASSERT_TRUE(wall / samples < 30.0);
  ASSERT_TRUE(worst < 30.0);
  PASS();
}
int main(void) {
  g_profile_passes = getenv("DC_RENDERER_PASS_PROFILE") != NULL;
  test_full_frame(480, 270);
  test_full_frame(640, 360);
  printf("Results: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
